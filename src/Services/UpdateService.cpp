#include "UpdateService.h"
#include "ConfigService.h"
#include "ProxyService.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QCoreApplication>
#include <QTimer>
#include <QRegularExpression>
#include <QNetworkProxy>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QCryptographicHash>

UpdateService::UpdateService(QObject *parent)
    : QObject(parent)
{
}

QString UpdateService::currentVersion()
{
    return QCoreApplication::applicationVersion();
}

// compare "v2.1.1" style tags; true if remote > local
bool UpdateService::versionGreater(const QString &remote, const QString &local)
{
    auto nums = [](const QString &s) {
        QStringList out;
        for (const QString &seg : s.split(QRegularExpression("[^0-9]+"), Qt::SkipEmptyParts))
            out << seg;
        return out;
    };
    QStringList r = nums(remote), l = nums(local);
    int n = qMax(r.size(), l.size());
    for (int i = 0; i < n; i++) {
        int rv = i < r.size() ? r[i].toInt() : 0;
        int lv = i < l.size() ? l[i].toInt() : 0;
        if (rv != lv) return rv > lv;
    }
    return false; // equal
}

void UpdateService::parseLatestRelease(const QByteArray &data)
{
    QJsonParseError pe;
    QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        m_lastError = "检查失败：无法解析服务器返回（可能是网络/代理问题）";
        emit checkFinished(false);
        return;
    }
    QJsonObject o = doc.object();
    QString tag = o.value("tag_name").toString();
    if (tag.isEmpty()) tag = o.value("name").toString();

    // find a release asset: prefer .zip (green edition, Defender-friendly), fallback .exe.
    // Capture the asset name and any sha256 digest GitHub reports for it.
    QString assetUrl;
    QString assetName;
    QString assetDigest;
    QJsonArray assets = o.value("assets").toArray();
    auto captureAsset = [&](const QJsonObject &a) {
        assetUrl = a.value("browser_download_url").toString();
        assetName = a.value("name").toString();
        QString d = a.value("digest").toString().trimmed();
        if (d.startsWith(QLatin1String("sha256:"), Qt::CaseInsensitive)) {
            QString hex = d.mid(7).trimmed().toLower();
            // whitelist: exactly 64 hex chars, nothing else is accepted
            static const QRegularExpression hex64("^[0-9a-f]{64}$");
            if (hex64.match(hex).hasMatch()) assetDigest = hex;
        }
    };
    for (const QJsonValue &v : assets) {
        QJsonObject a = v.toObject();
        if (a.value("name").toString().toLower().endsWith(".zip")) { captureAsset(a); break; }
    }
    if (assetUrl.isEmpty()) {
        for (const QJsonValue &v : assets) {
            QJsonObject a = v.toObject();
            if (a.value("name").toString().toLower().endsWith(".exe")) { captureAsset(a); break; }
        }
    }

    QString local = currentVersion();
    if (tag.isEmpty()) {
        m_lastError = "检查失败：服务器返回的版本信息为空";
        emit checkFinished(false);
        return;
    }
    if (!versionGreater(tag, local)) {
        // truly up to date — CLEAR any stale availability from an earlier check
        // (otherwise "发现新版本 vX" could linger after the user updated)
        m_lastError.clear();
        if (m_available || !m_latest.isEmpty()) {
            m_available = false;
            m_latest.clear();
            m_url.clear();
            m_assetName.clear();
            m_expectedSha256.clear();
            emit updateAvailableChanged();
        }
        emit checkFinished(false); // already up to date
        return;
    }
    m_latest = tag;
    m_url = assetUrl; // public repo: direct download works
    m_assetName = assetName;
    m_expectedSha256 = assetDigest; // may be empty; fall back to SHA256SUMS.txt later
    m_available = true;
    m_lastError.clear();
    emit updateAvailableChanged();
    emit checkFinished(true);
}

void UpdateService::checkForUpdates()
{
    // never clobber an in-flight download/verify/install flow
    if (m_downloading) return;

    // GitHub releases API (public repo, no token needed for read).
    QString api = "https://api.github.com/repos/XiaoqinOvo-UwU/xiaoqintools/releases/latest";

    m_lastError.clear();
    m_available = false;
    emit updateAvailableChanged();

    if (!m_mgr) m_mgr = new QNetworkAccessManager(this);
    // GitHub needs a proxy in CN. Prefer the system proxy (works for Clash
    // TUN/mixed mode and v2rayN), fall back to the known ports.
    m_mgr->setProxy(QNetworkProxy::applicationProxy()); // system proxy if any
    if (m_mgr->proxy().type() == QNetworkProxy::NoProxy)
        configureProxy(*m_mgr);

    QNetworkRequest req;
    req.setUrl(QUrl(api));
    req.setRawHeader("User-Agent", "XiaoQinTools");
    req.setRawHeader("Accept", "application/vnd.github+json");
    // follow 301/302 redirects (repo moved etc.)
    req.setMaximumRedirectsAllowed(5);
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    // abort if the connection stalls with no data flow
    req.setTransferTimeout(30000);
    QNetworkReply *reply = m_mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_lastError = "检查失败：" + reply->errorString();
            emit checkFinished(false);
            return;
        }
        parseLatestRelease(reply->readAll());
    });
}

void UpdateService::downloadAndInstall()
{
    if (m_downloading || m_url.isEmpty()) return;

    // ---- snapshot the request parameters: the check flow or a re-entrant
    // call must never mutate these mid-flight ----
    const QString url = m_url;
    const QString assetName = m_assetName;
    const QString expectedSha256 = m_expectedSha256;
    const QString tag = m_latest;

    // target dir: %TEMP%/XiaoQinToolsUpdate
    QString dir = QDir::temp().filePath("XiaoQinToolsUpdate");
    QDir().mkpath(dir);
    QString fileName = QUrl(url).fileName();
    if (fileName.isEmpty()) fileName = "update.exe";
    QString dest = dir + "/" + fileName;

    // busy flag covers the ENTIRE flow (hash fetch .. probe .. download ..
    // verify .. install), so a second click can never start a parallel chain
    m_downloading = true;
    m_progress = 0;
    emit downloadStateChanged();

    // hash resolution first: release JSON digest (captured at check time) or,
    // failing that, the official SHA256SUMS.txt for this release tag.
    if (!expectedSha256.isEmpty()) {
        startDownload(url, dest, expectedSha256, true /* official first, mirror fallback */);
        return;
    }
    fetchExpectedHashThenDownload(url, dest, tag, assetName);
}

// fetch the official checksum for the chosen asset, then start downloading.
// The checksum ALWAYS comes from GitHub (official), regardless of which mirror
// ends up serving the bytes.
void UpdateService::fetchExpectedHashThenDownload(const QString &url, const QString &dest,
                                                  const QString &tag, const QString &assetName)
{
    if (!m_mgr) m_mgr = new QNetworkAccessManager(this);
    if (m_mgr->proxy().type() == QNetworkProxy::NoProxy)
        configureProxy(*m_mgr);

    // SHA256SUMS.txt lives at the repo root for the release tag
    QString sumsUrl = "https://raw.githubusercontent.com/XiaoqinOvo-UwU/xiaoqintools/"
                      + tag + "/SHA256SUMS.txt";
    QNetworkRequest req{ QUrl(sumsUrl) };
    req.setRawHeader("User-Agent", "XiaoQinTools");
    req.setMaximumRedirectsAllowed(5);
    QNetworkReply *reply = m_mgr->get(req);
    // hard timeout: a stalled connection must not leave the flow busy forever
    QTimer::singleShot(8000, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [this, reply, url, dest, assetName]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            removeUpdateFiles(dest, QString());
            m_downloading = false;
            emit downloadStateChanged();
            emit downloadFinished(false, "无法从官方源获取校验值（" + reply->errorString() + "），已取消更新");
            return;
        }
        // parse "hexhash  filename" lines (strip a possible UTF-8 BOM first)
        QString content = QString::fromUtf8(reply->readAll());
        content.remove(QChar(0xFEFF));
        const QRegularExpression wsRe("\\s+");
        QString found;
        for (const QString &rawLine : content.split('\n')) {
            QString line = rawLine.trimmed();
            if (line.isEmpty()) continue;
            QStringList parts = line.split(wsRe);
            if (parts.size() < 2) continue;
            QString name = parts.last();
            if (name.startsWith('*')) name = name.mid(1);
            if (name == assetName) { found = parts.first().toLower(); break; }
        }
        if (found.isEmpty()) {
            removeUpdateFiles(dest, QString());
            m_downloading = false;
            emit downloadStateChanged();
            emit downloadFinished(false, "官方发布缺少该更新包的校验值，已取消更新（请联系维护者补发 SHA256SUMS）");
            return;
        }
        startDownload(url, dest, found, true);
    });
}

QStringList UpdateService::mirrorUrlsOnly(const QString &canonical) const
{
    // public GitHub proxy mirrors (only used if the official source fails)
    const QStringList prefix = {
        "https://ghproxy.net/",
        "https://gh-proxy.com/",
        "https://mirror.ghproxy.com/",
        "https://ghfast.top/",
        "https://ghproxy.cc/",
    };
    QStringList out;
    for (const QString &p : prefix)
        out << p + canonical;
    return out;
}

void UpdateService::configureProxy(QNetworkAccessManager &mgr)
{
    ProxyService probe;
    if (probe.isClashPortOpen())
        mgr.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 7897));
    else if (probe.isV2rayPortOpen())
        mgr.setProxy(QNetworkProxy(QNetworkProxy::HttpProxy, "127.0.0.1", 10808));
    else
        mgr.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
}

// ---- probe each candidate URL with a small ranged request, return the fastest ----
QString UpdateService::pickFastest(const QStringList &urls, int probeBytes, int timeoutMs)
{
    // Static: runs on a worker thread with NO access to members (no dangling
    // this if the service is destroyed mid-probe). Local manager, no parent.
    QNetworkAccessManager mgr;
    configureProxy(mgr);

    QString best;
    double bestSpeed = -1.0;
    for (const QString &u : urls) {
        QNetworkRequest req;
        req.setUrl(QUrl(u));
        req.setRawHeader("User-Agent", "XiaoQinTools");
        // request only the first probeBytes via a Range header
        req.setRawHeader("Range", QString("bytes=0-%1").arg(probeBytes - 1).toUtf8());

        QEventLoop loop;
        QElapsedTimer t;
        t.start();
        QNetworkReply *rep = mgr.get(req);
        qint64 got = 0;
        QObject::connect(rep, &QNetworkReply::readyRead, &loop, [&got, rep]() {
            got += rep->readAll().size();
        });
        QObject::connect(rep, &QNetworkReply::finished, &loop, [&loop, rep]() {
            rep->deleteLater();
            loop.quit();
        });
        QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
        loop.exec();

        qint64 ms = t.elapsed();
        rep->abort();
        rep->deleteLater();
        if (ms <= 0 || got <= 0) continue; // unreachable or nothing received
        double speed = (double)got * 1000.0 / (double)ms; // bytes/sec
        if (speed > bestSpeed) {
            bestSpeed = speed;
            best = u;
        }
    }
    return best;
}

// ---- download from a source, verify SHA-256, then install ----
void UpdateService::startDownload(const QString &url, const QString &dest,
                                  const QString &expectedSha256, bool mirrorFallback)
{
    m_progress = 0;
    emit downloadStateChanged();

    if (!m_mgr) m_mgr = new QNetworkAccessManager(this);
    if (m_mgr->proxy().type() == QNetworkProxy::NoProxy)
        configureProxy(*m_mgr);
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    req.setRawHeader("User-Agent", "XiaoQinTools");
    // abort only if NO data flows for 30s — slow-but-moving downloads are safe
    req.setTransferTimeout(30000);
    QNetworkReply *reply = m_mgr->get(req);
    QFile *out = new QFile(dest);
    if (!out->open(QIODevice::WriteOnly)) {
        delete out;
        m_downloading = false;
        emit downloadStateChanged();
        emit downloadFinished(false, "无法创建下载文件");
        return;
    }
    connect(reply, &QNetworkReply::readyRead, this, [reply, out]() {
        out->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        m_progress = total > 0 ? (int)(got * 100 / total) : 0;
        if (m_progress != m_lastLoggedProgress && (qAbs(m_progress - m_lastLoggedProgress) >= 10 || m_progress >= 100)) {
            qWarning("[update] download %d%% (got=%lld total=%lld)", m_progress, (long long)got, (long long)total);
            m_lastLoggedProgress = m_progress;
        }
        emit downloadStateChanged();
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, out, dest, expectedSha256, mirrorFallback]() {
        out->flush();
        out->close();
        delete out;
        reply->deleteLater();
        emit downloadStateChanged();

        // ---- source failed: only then consider mirrors ----
        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(dest);
            if (mirrorFallback) {
                QStringList mirrors = mirrorUrlsOnly(m_url);
                auto *w = new QFutureWatcher<QString>(this);
                connect(w, &QFutureWatcher<QString>::finished, this, [this, w, dest, expectedSha256]() {
                    QString best = w->result();
                    w->deleteLater();
                    if (best.isEmpty()) {
                        m_downloading = false;
                        emit downloadStateChanged();
                        emit downloadFinished(false, "所有下载源均不可用，请检查网络或代理");
                    } else {
                        startDownload(best, dest, expectedSha256, false); // mirror: no further fallback
                    }
                });
                // probe mirrors on a worker thread; captures only locals
                QFuture<QString> future = QtConcurrent::run(
                    [mirrors]() { return pickFastest(mirrors, 512 * 1024, 5000); });
                w->setFuture(future);
                return;
            }
            m_downloading = false;
            emit downloadStateChanged();
            emit downloadFinished(false, "下载失败：" + reply->errorString());
            return;
        }

        // ---- downloaded: NEVER execute before the official checksum matches ----
        // (hash computed + verified inside proceedToInstall's worker stage)
        proceedToInstall(dest, expectedSha256);
    });
}

// ---- after a verified download, extract (zip) or launch (exe) ----
void UpdateService::proceedToInstall(const QString &dest, const QString &expectedSha256)
{
    bool isZip = dest.endsWith(".zip", Qt::CaseInsensitive);

    // hash + (zip) extraction all run on a worker thread; the UI never blocks
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, dest, isZip]() {
        QString setupExe = watcher->result();
        watcher->deleteLater();
        if (setupExe == "HASH_MISMATCH") {
            removeUpdateFiles(dest, QString());
            m_downloading = false;
            emit downloadStateChanged();
            emit downloadFinished(false, "校验失败：下载内容与官方不一致，已取消安装");
            return;
        }
        if (setupExe.isEmpty()) {
            removeUpdateFiles(dest, QString());
            m_downloading = false;
            emit downloadStateChanged();
            emit downloadFinished(false, "更新包内容异常（未找到安装程序）");
            return;
        }
        // launch the installer; if the OS/AV refuses, do NOT silently quit
        if (!QProcess::startDetached(setupExe, QStringList() << "/VERYSILENT" << "/SUPPRESSMSGBOXES" << "/NORESTART")) {
            removeUpdateFiles(dest, QString());
            m_downloading = false;
            emit downloadStateChanged();
            emit downloadFinished(false, "启动安装程序失败（可能被安全软件拦截），已取消");
            return;
        }
        emit downloadFinished(true, isZip ? "校验通过，安装程序已启动~" : "更新包校验通过，安装程序已启动~");
        QTimer::singleShot(1500, qApp, &QCoreApplication::quit);
    });

    if (!isZip) {
        // exe path: verify hash on the worker, launch on success
        QFuture<QString> future = QtConcurrent::run([dest, expectedSha256]() {
            QString actual = sha256OfFile(dest);
            if (expectedSha256.isEmpty()) return QString("HASH_MISMATCH"); // fail-closed
            if (actual.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
                qWarning("[update] SHA-256 mismatch: expected=%s actual=%s",
                         qPrintable(expectedSha256), qPrintable(actual));
                return QString("HASH_MISMATCH");
            }
            qInfo("[update] SHA-256 verified OK");
            return dest; // verified -> launch this file
        });
        watcher->setFuture(future);
        return;
    }

    // zip path: work in a staging dir; verify + extract + locate setup.exe on
    // the worker thread, then launch on the main thread.
    QString staging = QDir::temp().filePath("XiaoQinToolsStage_" + QString::number(QCoreApplication::applicationPid()));
    QDir().mkpath(staging);
    QDir().mkpath(staging + "/new");
    emit downloadFinished(true, "校验通过，正在解压安装...");

    QFuture<QString> future = QtConcurrent::run([dest, staging, expectedSha256]() {
        // 0) checksum gate FIRST — nothing may be extracted/executed on mismatch
        QString actual = sha256OfFile(dest);
        if (expectedSha256.isEmpty()
            || actual.compare(expectedSha256, Qt::CaseInsensitive) != 0) {
            qWarning("[update] SHA-256 mismatch: expected=%s actual=%s",
                     qPrintable(expectedSha256), qPrintable(actual));
            return QString("HASH_MISMATCH");
        }
        qInfo("[update] SHA-256 verified OK");
        // 1) extract zip into staging/new
        QProcess tar;
        tar.start("tar", QStringList() << "-xf" << dest << "-C" << staging + "/new");
        tar.waitForFinished(120000);
        if (tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0)
            return QString();
        // 2) find the installer exe inside the zip
        QDirIterator it(staging + "/new", QStringList() << "XiaoQinTools-*-setup.exe" << "setup.exe",
                        QDir::Files, QDirIterator::Subdirectories);
        return it.hasNext() ? it.next() : QString();
    });
    watcher->setFuture(future);
}

QString UpdateService::sha256OfFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return QString();
    return QString::fromLatin1(h.result().toHex());
}

void UpdateService::removeUpdateFiles(const QString &dest, const QString &staging)
{
    if (!dest.isEmpty()) QFile::remove(dest);
    if (!staging.isEmpty()) {
        QDir d(staging);
        d.removeRecursively();
    }
}
