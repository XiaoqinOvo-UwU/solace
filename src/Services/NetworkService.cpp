// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "NetworkService.h"
#include "ProxyService.h"

#include <QProcess>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QUrl>
#include <QtConcurrent>
#include <QFutureWatcher>

NetworkService::NetworkService(QObject *parent)
    : QObject(parent)
{
}

QString NetworkService::pingHostSync(const QString &host, int timeoutMs)
{
    QProcess p;
    p.start("ping", QStringList() << "-n" << "1" << "-w" << QString::number(timeoutMs) << host);
    if (!p.waitForFinished(timeoutMs + 1000)) return "失败";
    QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    static const QRegularExpression re("[= ](\\d+)ms", QRegularExpression::CaseInsensitiveOption);
    auto m = re.match(out);
    return m.hasMatch() ? m.captured(1) + " ms" : "失败";
}

QString NetworkService::downloadSpeedMbpsSync(bool viaProxy, int bytes)
{
    Q_UNUSED(viaProxy) // QNetworkAccessManager uses system proxy automatically
    QString url = "https://speed.cloudflare.com/__down?bytes=" + QString::number(bytes);
    QNetworkAccessManager mgr;
    QNetworkRequest req;
    req.setUrl(QUrl(url));
    QNetworkReply *reply = mgr.get(req);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QElapsedTimer t;
    t.start();
    loop.exec();
    qint64 got = reply->bytesAvailable();
    double secs = t.nsecsElapsed() / 1e9;
    reply->deleteLater();
    if (secs <= 0) secs = 0.001;
    double mbps = got * 8.0 / 1e6 / secs;
    return QString::number(mbps, 'f', 2) + " Mbps";
}

QString NetworkService::diagnoseProxySync()
{
    ProxyService p;
    QStringList lines;
    lines << "=== 进程检测 ==="
          << ("Clash Verge: " + QString(p.isClashPortOpen() ? "运行中(端口通)" : "未运行/端口不通"))
          << ("v2rayN: " + QString(p.isV2rayPortOpen() ? "运行中(端口通)" : "未运行/端口不通"))
          << ""
          << "=== 端口检测 ==="
          << ("Clash (7897): " + QString(p.isClashPortOpen() ? "可连接 ●" : "不可连接 ✕"))
          << ("v2rayN (10808): " + QString(p.isV2rayPortOpen() ? "可连接 ●" : "不可连接 ✕"))
          << ""
          << "=== 连通性 ==="
          << ("直连百度: " + pingHostSync("www.baidu.com"));
    return lines.join("\n");
}

// ---- async wrappers (run heavy work in a thread pool, emit result) ----

void NetworkService::pingAsync(const QString &host)
{
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        emit result(watcher->result());
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([host]() {
        return pingHostSync(host);
    });
    watcher->setFuture(future);
}

void NetworkService::speedTestAsync(bool viaProxy, int bytes)
{
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        emit result(watcher->result());
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([viaProxy, bytes]() {
        return downloadSpeedMbpsSync(viaProxy, bytes);
    });
    watcher->setFuture(future);
}

void NetworkService::diagnoseAsync()
{
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        emit result(watcher->result());
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([]() {
        return diagnoseProxySync();
    });
    watcher->setFuture(future);
}

void NetworkService::nodeTestAsync()
{
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        emit result(watcher->result());
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([]() {
        QStringList lines;
        lines << "google.com: " + pingHostSync("www.google.com");
        lines << "youtube.com: " + pingHostSync("www.youtube.com");
        lines << "github.com: " + pingHostSync("github.com");
        return lines.join("\n");
    });
    watcher->setFuture(future);
}

// generic async JSON GET (home page cards). Tagged so one slot can serve
// several sources; empty result signals a network/API error.
void NetworkService::fetchJson(const QString &url, const QString &tag)
{
    if (!m_mgr)
        m_mgr = new QNetworkAccessManager(this);
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "XiaoQinTools");
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = m_mgr->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, tag]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit jsonReady(tag, QString());
            return;
        }
        emit jsonReady(tag, QString::fromUtf8(reply->readAll()));
    });
}
