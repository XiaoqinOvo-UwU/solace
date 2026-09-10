#include "AiService.h"
#include "ConfigService.h"
#include "ContactService.h"
#include "ContextManager.h"
#include "FactFilter.h"
#include "ResponseValidator.h"
#include "ConversationState.h"
#include "ConversationStateManager.h"
#include "MemoryRetriever.h"
#include "MemoryImportanceEvaluator.h"
#include "MemoryConflictManager.h"
#include "AppAnalyzer.h"
#include "DoNotDisturbManager.h"
#include "ProactiveScore.h"
#include "TopicGenerator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QRegularExpression>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QTimer>
#include <QMap>
#include <QRandomGenerator>
#include <algorithm>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

// forward decls for helpers defined later in this file
static QJsonObject readAiState();
static void writeAiState(const QJsonObject &o);
static void readPAD(QJsonObject *o, double *v, double *a, double *d);
static void integrateEmotion(QJsonObject *o, double dv, double da, double dd);
static void writeReflection(const QString &text);
static QString unfinishedPath();
static QString interestPath();
static QJsonObject readRelationship();
static bool looksLikeApiError(const QString &s);

AiService::AiService(QObject *parent)
    : QObject(parent)
    , m_ctx(new ContextManager)
    , m_convo(new ConversationState)
{
    ensureMemory();
    dedupMemoryNotes();
    const QString contactDir = ContactService::instance().contactDir(ContactService::instance().currentId());
    QDir().mkpath(contactDir);
    // v3.9 sidecars: conflict ledger + conversation resume point (memory.json untouched)
    MemoryConflictManager::setLedgerPath(contactDir + "/memory_meta.json");
    // v4.3: passive memory decay pass on startup (forgetting curve convergence)
    MemoryConflictManager::applyDecay();
    MemoryConflictManager::save();
    ConversationStateManager::setStatePath(contactDir + "/conversation_state.json");
    ConversationStateManager::load(*m_convo);
    // v4.3: daily reflection — once per day, think back and store a
    // first-person insight (her inner life, not user data). Runs AFTER the
    // conversation state is loaded so topicSummary is available.
    {
        QJsonObject ast = readAiState();
        const QString today = QDate::currentDate().toString(Qt::ISODate);
        if (ast.value("reflectionDate").toString() != today) {
            const QString summary = m_convo->topicSummary;
            if (!summary.isEmpty()) {
                QString rp = "昨天和你聊了这些：\n" + summary
                    + "\n\n请用一句第一人称的内心感想总结这段交流（不是复述事实，"
                      "而是你作为她此刻的真实感受，比如'我好像更懂他了'）。只输出这句话本身。";
                auto *rw = new QFutureWatcher<QString>(this);
                connect(rw, &QFutureWatcher<QString>::finished, this, [this, rw]() {
                    QString t = rw->result().trimmed();
                    rw->deleteLater();
                    // guard: skip empty / placeholder / error strings
                    if (t.isEmpty() || t.contains("还没配置 API Key") || t.size() >= 60)
                        return;
                    writeReflection(t);
                });
                QFuture<QString> rf = QtConcurrent::run([rp]() {
                    return callDeepSeekStatic("你是温柔可爱的AI陪伴者。", rp);
                });
                rw->setFuture(rf);
            }
        }
    }
    // v3.9.2: activity memory lives in its own file, NOT memory.json (personality stays clean)
    m_activityMemory.setPath(ConfigService::instance().configDir() + "/activity_memory.json");
    m_activityMemory.pruneOlderThan(30); // keep ~30 days
    // v3.9.2 phase 3: mood trend (config dir) + relationship state (contact dir)
    m_moodTrend.setPath(ConfigService::instance().configDir() + "/mood_trend.json");
    m_relationshipState.setPath(contactDir + "/relationship_state.json");
}

QString AiService::memoryPath() const
{
    // per-contact memory lives in %APPDATA% so it survives deleting the tool directory
    return ContactService::instance().contactMemoryPath(ContactService::instance().currentId());
}

void AiService::ensureMemory()
{
    QDir().mkpath(ContactService::instance().contactDir(ContactService::instance().currentId()));

    // migrate legacy memory files if present (first-run only, default contact)
    if (ContactService::instance().currentId().isEmpty()) {
        const QStringList legacy = {
            "C:/XiaoQinData/用户工具/ai_memory.json",
            "C:/deepseek杂货铺/小钦工具/ai_memory.json",
            "C:/XiaoQinData/tools-data/ai_memory.json",
        };
        if (!QFile::exists(memoryPath())) {
            for (const QString &old : legacy) {
                if (QFile::exists(old)) {
                    QFile::copy(old, memoryPath());
                    break;
                }
            }
        }
    }

    if (!QFile::exists(memoryPath())) {
        QFile f(memoryPath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QJsonObject o;
            o.insert("created", QDateTime::currentDateTime().toString(Qt::ISODate));
            f.write(QJsonDocument(o).toJson());
            f.close();
        }
    }
}

QString AiService::readMemory() const
{
    QFile f(memoryPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "{}";
    QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s.isEmpty() ? "{}" : s;
}

void AiService::writeMemory(const QString &json)
{
    QFile f(memoryPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(json.toUtf8());
        f.close();
    }
}

QString AiService::jsonGet(const QString &json, const QString &key)
{
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8());
    if (!d.isObject()) return QString();
    return d.object().value(key).toString();
}

QString AiService::jsonSet(const QString &json, const QString &key, const QString &value)
{
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    o.insert(key, value);
    return QString::fromUtf8(QJsonDocument(o).toJson());
}

// ---- session recording ----
void AiService::recordSessionStart()
{
    applyMemoryDecay(); // old memories fade into short gists (once per launch)
    if (!ConfigService::instance().allowTimeRecord()) return;
    QString mem = readMemory();
    QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
    // count yesterday's sessions for activity duration
    QDateTime now = QDateTime::currentDateTime();
    QString lastEnd = jsonGet(mem, "lastSessionEnd");
    if (!lastEnd.isEmpty()) {
        QDateTime le = QDateTime::fromString(lastEnd, Qt::ISODate);
        if (le.isValid()) {
            // sleep-time inference: the PC was off for a long stretch after the
            // last session ended and wasn't restarted in between — that's when
            // the user was asleep (any hour counts: they may nap at 6pm too).
            // 90+ minutes off excludes reboots and short breaks.
            qint64 gapSec = le.secsTo(now);
            if (gapSec >= 90 * 60) {
                int sh = le.time().hour();
                int sm = le.time().minute();
                QString sleepDate = le.date().toString("yyyy-MM-dd");
                // consecutive-day streak for late-night (凌晨) sleeps
                int streak = 1;
                if (sh < 5) {
                    QString prevDate = jsonGet(mem, "sleepLastDate");
                    if (!prevDate.isEmpty()
                        && QDate::fromString(prevDate, "yyyy-MM-dd").addDays(1).toString("yyyy-MM-dd") == sleepDate)
                        streak = jsonGet(mem, "sleepStreak").toInt() + 1;
                }
                mem = jsonSet(mem, "sleepHour", QString::number(sh));
                mem = jsonSet(mem, "sleepMin", QString::number(sm));
                mem = jsonSet(mem, "sleepGapMin", QString::number(gapSec / 60));
                mem = jsonSet(mem, "sleepLastDate", sleepDate);
                mem = jsonSet(mem, "sleepStreak", QString::number(streak));
                // keep the plain label for memory reports
                QString period = (sh < 5) ? "凌晨" : (sh < 11) ? "早上"
                               : (sh < 14) ? "中午" : (sh < 18) ? "下午" : "晚上";
                mem = jsonSet(mem, "sleepHabit", period + QString::number(sh) + "点"
                                + (sm >= 30 ? "半" : ""));
            }
            // shutdown/rest detection: long gap since the last session end means
            // the PC was off (or the user was away). Write a short-term memory note.
            qint64 gapHours = le.secsTo(now) / 3600;
            // only once per "previous session end" — relaunching several times
            // in a row must not append the same rest-note again
            if (gapHours >= 4 && jsonGet(mem, "lastRestNoteEnd") != lastEnd) {
                QString note = QString("用户休息了约 %1 小时（上次会话结束于 %2，本次开机 %3）")
                        .arg(gapHours)
                        .arg(le.toString("MM-dd HH:mm"))
                        .arg(now.toString("MM-dd HH:mm"));
                appendNote(note);
                mem = readMemory(); // appendNote rewrote the file
                mem = jsonSet(mem, "lastRestNoteEnd", lastEnd);
                writeMemory(mem);
            }
        }
    }
    mem = jsonSet(mem, "lastSessionStart", now.toString(Qt::ISODate));
    mem = jsonSet(mem, "lastDate", today);
    writeMemory(mem);
}

void AiService::recordSessionEnd()
{
    if (!ConfigService::instance().allowTimeRecord()) return;
    QString mem = readMemory();
    QString end = QDateTime::currentDateTime().toString(Qt::ISODate);
    mem = jsonSet(mem, "lastSessionEnd", end);

    // flush any tracked app usage into memory before exit
    flushActivityToMemory();

    // accumulate usage minutes for today
    QString start = jsonGet(mem, "lastSessionStart");
    if (!start.isEmpty()) {
        QDateTime st = QDateTime::fromString(start, Qt::ISODate);
        if (st.isValid()) {
            qint64 mins = st.secsTo(QDateTime::currentDateTime()) / 60;
            QString today = QDateTime::currentDateTime().toString("yyyy-MM-dd");
            QString totalKey = "usageMinutes_" + today;
            int total = jsonGet(mem, totalKey).toInt() + (int)mins;
            mem = jsonSet(mem, totalKey, QString::number(total));
        }
    }
    writeMemory(mem);
}

// ---- PC activity monitor ----
void AiService::startActivityMonitor()
{
    if (m_monitorTimer) return; // already running
    if (!ConfigService::instance().allowTimeRecord()) return; // privacy: no usage tracking
    m_monitorTimer = new QTimer(this);
    m_monitorTimer->setInterval(60000); // sample every minute
    connect(m_monitorTimer, &QTimer::timeout, this, [this]() { recordActivitySample(); });
    m_monitorTimer->start();
    recordActivitySample(); // immediate first sample
}

void AiService::stopActivityMonitor()
{
    if (m_monitorTimer) {
        m_monitorTimer->stop();
        m_monitorTimer->deleteLater();
        m_monitorTimer = nullptr;
    }
    flushActivityToMemory();
}

// executable name of the foreground window (win32), "" on failure — used to
// classify via AppAnalyzer by process, not just window title.
static QString foregroundProcessExe()
{
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return QString();
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid) return QString();
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return QString();
    wchar_t buf[1024] = {0};
    DWORD len = 1024;
    BOOL ok = QueryFullProcessImageNameW(h, 0, buf, &len);
    CloseHandle(h);
    if (!ok) return QString();
    return QFileInfo(QString::fromWCharArray(buf, len)).fileName().toLower();
#else
    return QString();
#endif
}

void AiService::recordActivitySample()
{
    QString app = foregroundApp();
    QString exe = foregroundProcessExe();
    if (app.isEmpty() || AppAnalyzer::isExcluded(exe, app)) {
        m_fgApp.clear();
        return;
    }
    qint64 now = QDateTime::currentDateTime().toMSecsSinceEpoch();
    if (m_fgApp != app) {           // foreground changed -> restart the session clock
        m_fgApp = app;
        m_fgSinceMs = now;
    }
    // v3.9.2: detect "just finished a task" — leaving gaming/coding for
    // browsing/chatting is a natural moment to say something.
    {
        AppAnalysis a = AppAnalyzer::analyze(exe, app, 0);
        if (!a.isSystemNoise && !m_lastFgCategory.isEmpty() && m_lastFgCategory != a.category) {
            const bool wasWork = (m_lastFgCategory == "gaming" || m_lastFgCategory == "creating"
                                  || m_lastFgCategory == "terminal");
            const bool nowRelax = (a.category == "browsing" || a.category == "chatting"
                                   || a.category == "media" || a.category == "other");
            if (wasWork && nowRelax) {
                m_justFinishedTask = true;
                m_justFinishedTaskAtMs = now;
            }
        }
        m_lastFgCategory = a.category;
    }
    m_appMinutes[app] = m_appMinutes.value(app) + 1;
    // persist every 10 minutes so a crash doesn't lose everything
    if ((m_appMinutes.size() % 10) == 0 || m_appMinutes.value(app) >= 10)
        flushActivityToMemory();
}

void AiService::flushActivityToMemory()
{
    if (m_appMinutes.isEmpty()) return;
    // push accumulated minutes into the SEPARATE activity memory (never memory.json)
    for (auto it = m_appMinutes.constBegin(); it != m_appMinutes.constEnd(); ++it)
        m_activityMemory.recordMinutes(it.key(), it.value());
    m_activityMemory.setUptime(uptimeText());
    m_activityMemory.pruneOlderThan(30); // keep ~30 days
    m_appMinutes.clear();
}

QString AiService::activitySummary()
{
    QString top = m_activityMemory.todayTopApps(3, true);
    if (top.isEmpty()) return "（今天还没怎么用电脑）";
    return "今天大部分时间在：" + top;
}

// top-used app today (excl. system/proxy/explorer) — "当前运行最久"
QString AiService::longestRunningAppToday()
{
    return m_activityMemory.todayTopApps(1, true);
}

// live "you're using X, for ~Y minutes" from the current foreground session
QString AiService::currentForegroundSessionText()
{
    if (m_fgApp.isEmpty() || m_fgSinceMs <= 0) return QString();
    qint64 mins = (QDateTime::currentDateTime().toMSecsSinceEpoch() - m_fgSinceMs) / 60000;
    if (mins < 1) return QString();
    return QString("你正在使用 %1，已经用了约 %2 分钟。").arg(m_fgApp).arg(mins);
}

// yesterday's archived top apps (from activity_memory.json)
QString AiService::yesterdayActivitySummary()
{
    QString top = m_activityMemory.yesterdayTopApps(3, true);
    return top.isEmpty() ? QString() : "昨天你主要用了：" + top;
}

// home "AI 日报": yesterday's usage / top app / shutdown time + a short tip.
// Returns raw values as JSON — the QML home page owns presentation (emoji,
// layout, chat-duration line). Missing values come back as 0 / "".
QString AiService::dailyReportData()
{
    const QDate y = QDate::currentDate().addDays(-1);
    const QJsonObject mo = QJsonDocument::fromJson(readMemory().toUtf8()).object();

    // total computer time yesterday (activity memory first, memory.json fallback)
    int mins = m_activityMemory.dayMinutes(y);
    if (mins <= 0)
        mins = mo.value("usageMinutes_" + y.toString("yyyy-MM-dd")).toInt();

    // top app yesterday: "Minecraft（180 分钟）"
    QString topApp;
    int topAppMin = 0;
    const QString top = m_activityMemory.dayTopApps(y, 1, true);
    if (!top.isEmpty()) {
        const int p = top.indexOf("（");
        if (p > 0) {
            topApp = top.left(p);
            const int q = top.indexOf("分钟", p);
            if (q > p) topAppMin = top.mid(p + 1, q - p - 1).toInt();
        } else {
            topApp = top;
        }
    }

    // non-entertainment time yesterday ("工作/学习"): every app category except
    // gaming / media / chatting
    int workMin = 0;
    {
        const QJsonObject apps = m_activityMemory.dayApps(y);
        for (auto it = apps.constBegin(); it != apps.constEnd(); ++it) {
            // activity-memory keys are window TITLES, so classify by the title
            // (an empty exe made everything fall through to "other" == uptime)
            const AppAnalysis a = AppAnalyzer::analyze(it.key(), it.key(), 0);
            if (a.isSystemNoise) continue;
            const QString c = a.category;
            // 工作/学习 = coding / terminal / documents / browsing.
            // gaming / media / chatting are entertainment; "other"/"noise" are
            // unknown or idle and are NEVER counted, so uptime can't inflate it.
            if (c == "creating" || c == "terminal" || c == "document" || c == "browsing")
                workMin += it.value().toInt();
        }
    }

    // shutdown time: sleep inference first, then the last recorded session end
    QString shutdown;
    int sleepHour = -1;
    const QString ymd = y.toString("yyyy-MM-dd");
    if (mo.value("sleepLastDate").toString() == ymd) {
        sleepHour = mo.value("sleepHour").toInt();
        shutdown = QString("昨晚 %1:%2 关闭电脑")
            .arg(sleepHour, 2, 10, QChar('0'))
            .arg(mo.value("sleepMin").toInt(), 2, 10, QChar('0'));
    } else {
        const QDateTime end = QDateTime::fromString(mo.value("lastSessionEnd").toString(), Qt::ISODate);
        if (end.isValid())
            shutdown = (end.date() == y)
                ? QString("昨晚 %1 关闭电脑").arg(end.toString("HH:mm"))
                : QString("上次关机 %1").arg(end.toString("MM-dd HH:mm"));
    }

    // advice: today's AI-written tip if already generated, otherwise a local
    // heuristic until the model answers (kept stable for the whole day)
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QString advice = mo.value("dailyAdvice").toString().trimmed();
    if (advice.isEmpty() || mo.value("dailyAdviceDate").toString() != today) {
        if (sleepHour >= 0 && sleepHour < 5)
            advice = "昨天睡得有点晚，今天可能会比较困，中午补个觉吧。";
        else if (mins >= 8 * 60)
            advice = "昨天在电脑前坐了很久，今天记得多起来活动活动。";
        else if (mins > 0 && mins < 2 * 60)
            advice = "昨天几乎没怎么碰电脑，是好好休息了吧~";
        else
            advice = "今天也要元气满满哦。";
    }

    QJsonObject o;
    o.insert("usageMinutes", mins);
    o.insert("topApp", topApp);
    o.insert("topAppMinutes", topAppMin);
    o.insert("workMinutes", workMin);
    o.insert("shutdown", shutdown);
    o.insert("advice", advice);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// home "AI 日报" tip: ask the model for one short line based on yesterday's
// real usage. Falls back silently (QML keeps the local heuristic) when no key
// or the request fails.
void AiService::generateDailyAdvice()
{
    const QString today = QDate::currentDate().toString("yyyy-MM-dd");

    // stable for the whole day: reuse the cached tip instead of re-asking the
    // model on every visit (non-deterministic answers looked "random")
    {
        const QJsonObject mo = QJsonDocument::fromJson(readMemory().toUtf8()).object();
        const QString cached = mo.value("dailyAdvice").toString().trimmed();
        if (!cached.isEmpty() && mo.value("dailyAdviceDate").toString() == today) {
            emit adviceReady(cached);
            return;
        }
    }
    if (ConfigService::instance().apiKey().trimmed().isEmpty()) return;

    const QDate y = QDate::currentDate().addDays(-1);
    const QJsonObject mo = QJsonDocument::fromJson(readMemory().toUtf8()).object();
    int mins = m_activityMemory.dayMinutes(y);
    if (mins <= 0)
        mins = mo.value("usageMinutes_" + y.toString("yyyy-MM-dd")).toInt();
    const QString top = m_activityMemory.dayTopApps(y, 1, false);
    int sleepHour = -1;
    if (mo.value("sleepLastDate").toString() == y.toString("yyyy-MM-dd"))
        sleepHour = mo.value("sleepHour").toInt();

    const QString facts = QString("昨天用户使用电脑约 %1 小时，最常用的是「%2」，大约 %3 点结束使用电脑。")
        .arg(mins / 60)
        .arg(top.isEmpty() ? QString("没什么") : top)
        .arg(sleepHour >= 0 ? QString::number(sleepHour) : QString("不详"));

    const QString prompt = facts
        + "\n请以用户的AI伙伴身份写一句今天的状态建议：先用半句说明理由（依据上面的情况），"
          "再给出建议；总长不超过 32 个字，温柔口语，不要括号或动作描写，不要复述具体数字，"
          "不要换行，只输出这句话本身。";

    auto *w = new QFutureWatcher<QString>(this);
    connect(w, &QFutureWatcher<QString>::finished, this, [this, w, today]() {
        QString t = w->result().trimmed();
        w->deleteLater();
        if (t.isEmpty() || looksLikeApiError(t)) return;
        t.remove(QRegularExpression("^[\"'“”‘’\\s]+|[\"'“”‘’\\s]+$"));
        if (t.isEmpty() || t.size() > 60) return;
        setApiOnline(true);
        // persist for the day so it never changes on later visits
        QString mem = readMemory();
        mem = jsonSet(mem, "dailyAdvice", t);
        mem = jsonSet(mem, "dailyAdviceDate", today);
        writeMemory(mem);
        emit adviceReady(t);
    });
    QFuture<QString> f = QtConcurrent::run([prompt]() {
        return callDeepSeekStatic("你是温柔可爱的AI陪伴者。", prompt);
    });
    w->setFuture(f);
}

// ---- v3.9.2: silence-gate accessors ----
bool AiService::isDoNotDisturb() { return !m_dndReason.isEmpty(); }
QString AiService::doNotDisturbReason() { return m_dndReason; }
int AiService::lastProactiveScore() { return m_lastProactiveScore; }

// ---- appearance mode (wallpaper glass) — passthrough to ConfigService ----
QString AiService::appearanceMode() { return ConfigService::instance().appearanceMode(); }
void AiService::setAppearanceMode(const QString &v) { ConfigService::instance().setAppearanceMode(v == "glass" ? "glass" : QString()); }
double AiService::wallpaperGlassOpacity() { return ConfigService::instance().wallpaperGlassOpacity(); }
void AiService::setWallpaperGlassOpacity(double v) { ConfigService::instance().setWallpaperGlassOpacity(v); }

// average colour of the wallpaper — drives the glass-mode environment tint so
// the wallpaper's hue bleeds into the frosted panels ("环境色融合").
QString AiService::wallpaperTintColor()
{
    const QString sharp = ConfigService::instance().configDir() + "/wallpaper.png";
    if (!QFile::exists(sharp)) return "#FFFFFF";
    QImage img(sharp);
    if (img.isNull()) return "#FFFFFF";
    const QImage small = img.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return small.pixelColor(0, 0).name();
}

// ---- novel edit counting (scan C:\AI库\小说 for yesterday-modified .md files) ----
static int countNovelEditsYesterday()
{
    QDir dir("C:/AI库/小说");
    if (!dir.exists()) return 0;
    QDate yesterday = QDate::currentDate().addDays(-1);
    int count = 0;
    const auto files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : files) {
        if (fi.suffix().compare("md", Qt::CaseInsensitive) == 0 && fi.lastModified().date() == yesterday)
            count++;
    }
    return count;
}

// ---- daily greeting gate (once per day) ----
bool AiService::shouldGreetToday()
{
    QString mem = readMemory();
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    return jsonGet(mem, "lastGreetDate") != today;
}

void AiService::markGreeted()
{
    QString mem = readMemory();
    mem = jsonSet(mem, "lastGreetDate", QDate::currentDate().toString("yyyy-MM-dd"));
    writeMemory(mem);
}

// ---- greeting ----
static QString cnHour(int h)
{
    static const char *nums[] = { "零","一","二","三","四","五","六","七","八","九","十",
        "十一","十二","十三","十四","十五","十六","十七","十八","十九","二十",
        "二十一","二十二","二十三" };
    if (h >= 0 && h <= 23) return QString::fromUtf8(nums[h]);
    return QString::number(h);
}

QString AiService::greeting()
{
    QString mem = readMemory();
    QDateTime now = QDateTime::currentDateTime();
    int hour = now.time().hour();
    QString uname = ConfigService::instance().userName();
    if (uname.isEmpty()) uname = "用户";

    QStringList lines;
    if (hour >= 5 && hour < 11)
        lines << "早上好，" + uname + "。";
    else if (hour >= 11 && hour < 14)
        lines << "中午好，" + uname + "。";
    else if (hour >= 14 && hour < 18)
        lines << "下午好，" + uname + "。";
    else if (hour >= 18 && hour < 23)
        lines << "晚上好，" + uname + "。";
    else
        lines << "这么晚了还没睡……" + uname + "？";

    // yesterday usage minutes
    QDate yesterday = QDate::currentDate().addDays(-1);
    QString key = "usageMinutes_" + yesterday.toString("yyyy-MM-dd");
    int mins = jsonGet(mem, key).toInt();
    if (mins > 0) {
        int h = mins / 60, m = mins % 60;
        if (h >= 1) lines << QString("昨天你使用电脑 %1 小时 %2 分钟。").arg(h).arg(m);
        else lines << QString("昨天你使用电脑 %1 分钟。").arg(m);
    }

    // novel edits yesterday
    int edits = countNovelEditsYesterday();
    if (edits > 0)
        lines << QString("你的小说文档昨天修改了 %1 次。").arg(edits);

    // sleep habit — categorized by when the user actually slept last night
    {
        QString sleepDate = jsonGet(mem, "sleepLastDate");
        QString yesterday = QDate::currentDate().addDays(-1).toString("yyyy-MM-dd");
        if (sleepDate == yesterday) {
            int sh = jsonGet(mem, "sleepHour").toInt();
            int sm = jsonGet(mem, "sleepMin").toInt();
            qint64 gapMin = jsonGet(mem, "sleepGapMin").toLongLong();
            int streak = jsonGet(mem, "sleepStreak").toInt();
            if (sh < 5) {                       // 凌晨
                if (streak >= 3)
                    lines << "另外……你又熬夜了，连续好几天了，注意身体哦";
                else
                    lines << "另外，你昨天晚上凌晨睡觉，下次不要这样了哦";
            } else if (sh < 11) {               // 早上
                lines << "又通宵了？你呀，今天犯困可得靠自己了哦";
            } else if (sh < 14) {               // 中午
                if (gapMin >= 6 * 60)
                    lines << "另外，你昨天中午就睡了，直接睡到了第二天，看来是真累了";
                else
                    lines << "另外，你昨天中午睡了个午觉，休息得怎么样呀？";
            } else if (sh < 18) {               // 下午
                lines << "看来昨天很累呢，睡得很早哦";
            } else {                            // 晚上 18-23：正常睡眠，标出时间
                lines << "另外，你昨天晚上" + cnHour(sh) + (sm >= 30 ? "点半" : "点")
                         + "睡觉，很好的习惯哦";
            }
        }
    }

    // system uptime
    QString up = uptimeText();
    if (!up.isEmpty())
        lines << up;

    // v3.9.1: sometimes bring up yesterday's top apps to start a topic (随机)
    if (QRandomGenerator::global()->bounded(100) < 35) {
        QString y = yesterdayActivitySummary();
        if (!y.isEmpty())
            lines << y;
    }

    if (lines.size() == 1) // nothing remembered yet
        lines << "今天也要元气满满哦 Ovo";

    return lines.join("\n");
}

// ---- memory report (human-readable) ----
QString AiService::memoryReport()
{
    QJsonDocument d = QJsonDocument::fromJson(readMemory().toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QStringList lines;

    QString created = o.value("created").toString();
    if (!created.isEmpty())
        lines << "首次使用：" + created.replace('T', ' ').left(16);

    QString lastDate = o.value("lastDate").toString();
    if (!lastDate.isEmpty())
        lines << "最近使用：" + lastDate;

    // sleep habit
    QString sleep = o.value("sleepHabit").toString();
    if (!sleep.isEmpty())
        lines << "睡眠习惯：" + sleep + "睡觉";

    // usage minutes for recent days
    lines << "";
    lines << "每日使用时长：";
    int shown = 0;
    for (int i = 3; i >= 0; i--) {
        QString day = QDate::currentDate().addDays(-i).toString("yyyy-MM-dd");
        QString key = "usageMinutes_" + day;
        int mins = o.value(key).toInt();
        if (mins > 0) {
            int h = mins / 60, m = mins % 60;
            lines << (h > 0 ? QString("  %1：%2 小时 %3 分钟").arg(day).arg(h).arg(m)
                            : QString("  %1：%2 分钟").arg(day).arg(m));
            shown++;
        }
    }
    if (shown == 0)
        lines << "  （暂无时长记录）";

    return lines.join("\n");
}

// ---- system uptime (Windows) ----
QString AiService::uptimeText()
{
#ifdef Q_OS_WIN
    ULONGLONG ms = GetTickCount64();
    qint64 secs = (qint64)(ms / 1000);
    qint64 h = secs / 3600, m = (secs % 3600) / 60;
    return QString("电脑已开机 %1 小时 %2 分钟。").arg(h).arg(m);
#else
    return QString();
#endif
}

// ---- current foreground window (lightweight, no process scan) ----
QString AiService::foregroundApp()
{
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return QString();
    wchar_t buf[512];
    int len = GetWindowTextW(hwnd, buf, 512);
    if (len <= 0) return QString();
    QString t = QString::fromWCharArray(buf, len).trimmed();
    if (t.isEmpty()) return QString();
    // skip our own window title so the AI doesn't think it's "小钦的工具"
    if (t.contains("小钦的工具")) return QString();
    return t;
#else
    return QString();
#endif
}

// ---- idle detection: game running? ----
bool AiService::isGameRunning()
{
    // common games the user plays — NOTE: steam.exe is a background client,
    // not a game; treating it as "gaming" blocked idle chat entirely
    QStringList games = { "r5apex.exe", "javaw.exe", "java.exe", "Minecraft" };
    QProcess p;
    p.start("tasklist", QStringList() << "/FO" << "CSV");
    if (!p.waitForFinished(1500)) return false;
    QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    for (const QString &g : games)
        if (out.contains(g, Qt::CaseInsensitive)) return true;
    return false;
}

// ---- is the foreground window a fullscreen game? (any game, not just a known list) ----
bool AiService::isFullscreenGame()
{
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;
    if (!IsWindowVisible(hwnd)) return false;

    // ignore our own window
    DWORD ourPid = GetCurrentProcessId();
    DWORD winPid = 0;
    GetWindowThreadProcessId(hwnd, &winPid);
    if (winPid == ourPid) return false;

    RECT r;
    if (!GetClientRect(hwnd, &r)) return false;
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    // treat as fullscreen when it covers the primary screen (>= 98% of each dimension)
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    return w >= sw * 98 / 100 && h >= sh * 98 / 100;
#else
    return false;
#endif
}

// ---- is the foreground window Minecraft? ----
// MC Java (including Chinese-named mod packs) runs on javaw.exe/java.exe,
// so we detect by the foreground window's process rather than the title alone.
bool AiService::isForegroundMinecraft()
{
#ifdef Q_OS_WIN
    HWND hwnd = GetForegroundWindow();
    if (!hwnd) return false;
    if (!IsWindowVisible(hwnd)) return false;
    DWORD ourPid = GetCurrentProcessId();
    DWORD winPid = 0;
    GetWindowThreadProcessId(hwnd, &winPid);
    if (winPid == ourPid) return false;

    // check the foreground window's process name first
    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, winPid);
    if (hProc) {
        wchar_t pbuf[512];
        DWORD n = 512;
        if (QueryFullProcessImageNameW(hProc, 0, pbuf, &n)) {
            QString path = QString::fromWCharArray(pbuf, (int)n);
            QString base = QFileInfo(path).fileName().toLower();
            CloseHandle(hProc);
            if (base == "javaw.exe" || base == "java.exe")
                return true; // Java game foreground -> treat as Minecraft
        } else {
            CloseHandle(hProc);
        }
    }

    // fallback: window title contains "Minecraft" (vanilla or english launchers)
    wchar_t buf[512];
    int len = GetWindowTextW(hwnd, buf, 512);
    if (len <= 0) return false;
    QString t = QString::fromWCharArray(buf, len).trimmed();
    return t.contains("Minecraft", Qt::CaseInsensitive);
#else
    return false;
#endif
}

// ---- system-wide idle: ms since the last keyboard/mouse input ----
qint64 AiService::lastInputMs()
{
#ifdef Q_OS_WIN
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (!GetLastInputInfo(&lii)) return -1;
    // GetTickCount wraps at ~49 days; guard against a stale read
    DWORD now = GetTickCount();
    DWORD delta = now - lii.dwTime; // unsigned wraps safely
    if (delta > 3600u * 24 * 1000) return -1; // > 1 day: treat as unknown
    return static_cast<qint64>(delta);
#else
    return -1;
#endif
}

// ---- idle chat: AI proactively starts a topic (unified pipeline) ----
// idleChat -> ContextManager (system data) -> MemoryRetriever (recall)
//          -> TopicSelector (rankTopics) -> PromptBuilder -> DeepSeek
void AiService::idleChat()
{
    // ---- v3.9.2: silence gate (DoNotDisturb + ProactiveScore) ----
    // Whether to talk is decided by real desktop state + a deterministic
    // formula — NEVER by the LLM. Stay silent while gaming / fullscreen /
    // meeting / deep-work, or when the proactive score is below 50.
    if (!ConfigService::instance().allowStateRead() || !ConfigService::instance().allowTimeRecord())
        return;
    {
        const QString exe = foregroundProcessExe();
        const QString fgTitle = foregroundApp();
        AppAnalysis fg = AppAnalyzer::analyze(exe, fgTitle, 0);
        const bool fullscreen = isFullscreenGame();
        const bool gameProc = isGameRunning() || fg.category == "gaming";
        DoNotDisturbState dnd = DoNotDisturbManager::evaluate(fg.category, exe, fullscreen, gameProc);
        if (dnd.enabled) {
            m_dndReason = dnd.reason;
            emit doNotDisturbChanged();
            return; // silent
        }
        m_dndReason.clear();
        emit doNotDisturbChanged();

        // "user refused" cooldown: counts for 30 minutes
        if (m_refusedAtMs > 0 &&
            QDateTime::currentDateTime().toMSecsSinceEpoch() - m_refusedAtMs > 30 * 60 * 1000)
            m_userJustRefused = false;
        // "just finished a task" stays valid for ~45 min
        if (m_justFinishedTaskAtMs > 0 &&
            QDateTime::currentDateTime().toMSecsSinceEpoch() - m_justFinishedTaskAtMs > 45 * 60 * 1000)
            m_justFinishedTask = false;

        int hour = QTime::currentTime().hour();
        ProactiveInput in;
        in.idleMs = lastInputMs();
        in.lateNight = (hour >= 23 || hour < 5);
        in.gaming = fg.category == "gaming";
        in.coding = (fg.category == "creating" || fg.category == "terminal");
        in.fullscreen = fullscreen;
        in.justRefused = m_userJustRefused;
        in.justFinishedTask = m_justFinishedTask;
        const QString ce = m_convo->userEmotion;
        in.userMoodLow = (ce == "tired" || ce == "stressed" || ce == "sad" || ce == "lonely");

        m_lastProactiveScore = ProactiveScore::compute(in);
        m_justFinishedTask = false; // consumed
        if (m_lastProactiveScore < 45) return; // stay quiet

        // ---- v4.3: proactive quality gates (读空气 + 冷却 + 间隔) ----
        const qint64 nowMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
        // 1) read the air: if the user ignored the last 2 proactive messages
        //    (no reply at all), the AI backs off for a long while instead of
        //    pestering. Any user reply resets the counter.
        if (m_unansweredProactive >= 2) {
            // needs a real user reply (within 10 min of a proactive message)
            const qint64 sinceUser = nowMs - m_lastUserReplyAtMs;
            if (sinceUser > 0 && sinceUser < 10 * 60 * 1000) {
                m_unansweredProactive = 0; // they responded — forgiven
            } else if (m_lastProactiveAtMs > 0 && nowMs - m_lastProactiveAtMs < 2 * 60 * 60 * 1000) {
                return; // back off ~2h after 2 ignored messages
            }
        }
        // 2) cooldown: never send two proactive messages within 25 minutes
        if (m_lastProactiveAtMs > 0 && nowMs - m_lastProactiveAtMs < 25 * 60 * 1000)
            return;
    }

    QString user = ConfigService::instance().userName();
    QString ai = ContactService::instance().currentName();
    QString fg = foregroundApp();         // cheap win32 call
    QString recent = m_chatBuffer.join("\n"); // last few messages if any

    // proactive engine inputs: state, unfinished topics, interests, events
    QString state = userActivityState();
    QString aiState = aiStateJson();
    QStringList unfinished = unfinishedTopicsText(5).split('\n', Qt::SkipEmptyParts);
    QStringList interests = interestsText(5).split('\n', Qt::SkipEmptyParts);
    QStringList events = eventMemoryText(4).split('\n', Qt::SkipEmptyParts);
    QStringList userFacts;
    {
        // user's own recent stated facts (verified)
        QList<Fact> uf = m_ctx->factsBySource(FactSource::UserMessage);
        int n = qMin(uf.size(), 5);
        for (int i = 0; i < n; ++i)
            userFacts << uf.at(uf.size() - 1 - i).content;
    }

    // ---- feed real system data into the shared ContextManager so the same
    //      recall + fact gate applies to proactive chat as to replies ----
    QStringList sysFacts;
    if (ConfigService::instance().allowStateRead()) {
        if (!state.isEmpty() && state != "active") {
            m_ctx->addSystemData(state, 0.95, {"state"});
            sysFacts << QString("- [system_data] 用户当前状态：%1").arg(state);
        }
        if (!fg.isEmpty()) {
            m_ctx->addSystemData("用户当前前台窗口是 " + fg, 0.90, {"foreground"});
            sysFacts << QString("- [system_data] 用户当前前台窗口是 %1").arg(fg);
        }
        // v3.9.1: current session + today's top apps give the AI depth to
        // start a topic ("你用了X挺久了吧" / "今天Y玩得不少") without guessing.
        QString cur = currentForegroundSessionText();
        if (!cur.isEmpty()) {
            m_ctx->addSystemData(cur, 0.90, {"foreground"});
            sysFacts << QString("- [system_data] %1").arg(cur);
        }
        QString longest = longestRunningAppToday();
        if (!longest.isEmpty())
            sysFacts << QString("- [system_data] 今天用得最多：%1").arg(longest);
    }

    // ---- MemoryRetriever: recall memories relevant to the current situation ----
    QString memBlock;
    {
        const QString query = (state + " " + fg + " " + recent).simplified();
        QList<Fact> rec = m_ctx->retrieveMemories(query, m_convo ? m_convo->topic : QString(), 6);
        QStringList lines;
        for (const Fact &f : rec) {
            lines << QString("- [%1] %2").arg(f.memKindName(), f.content);
            MemoryConflictManager::bumpUsage(f.content);
        }
        MemoryConflictManager::save();
        memBlock = lines.join("\n");
    }

    // ---- TopicGenerator: 10 candidates from real sources, scored on
    //      relevance/freshness/relationship/interruptCost; pick the top 3 ----
    const double interruptPenalty = (state == "active" || state == "coding") ? 0.4 : 0.0;
    QList<TopicCandidate> topics = TopicGenerator::generate(
        userFacts, unfinished, interests, events, sysFacts, interruptPenalty);
    QStringList topTopics;
    int nt = qMin(topics.size(), 3);
    for (int i = 0; i < nt; ++i)
        topTopics << QString("- %1（%2）").arg(topics.at(i).topic, topics.at(i).source);

    // v4.3 thread-safety: openLoopPromptBlock reads the static ledger on the
    // MAIN thread; the worker lambda only receives the precomputed string.
    const QString loopBlock = openLoopPromptBlock();

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        QString raw = watcher->result();
        // ResponseValidator gate for proactive messages too
        ResponseValidator::Result vr = ResponseValidator::validate(raw, QString());
        QString text = vr.text;
        // AI may decide to stay quiet (e.g. user is coding) -> skip
        if (text.isEmpty()) {
            watcher->deleteLater();
            return;
        }
        m_lastAiReply = text;
        // v4.3: track proactive message for read-the-air + cooldown
        m_lastProactiveAtMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
        m_unansweredProactive++;
        // both: show in chat, and flag as a proactive (idle) message
        setApiOnline(!looksLikeApiError(text));
        emit chatReply(text);
        emit idleReply(text);
        watcher->deleteLater();
    });
    // collect the process snapshot in the worker thread (one tasklist call,
    // only on idle trigger — zero cost during normal use)
    QFuture<QString> future = QtConcurrent::run(
        [user, ai, fg, recent, state, aiState, topTopics, sysFacts, memBlock, loopBlock, this]() {
        QString prompt = "你是" + ai + "，人设：" + aiPersonality() + "。用户" + user + "已经有一会儿没操作电脑了。\n"
            + "【我们的关系】" + relationshipText() + "\n"
            + "【已验证事实】（仅系统真实检测）\n"
            + (sysFacts.isEmpty() ? QString("- （用户关闭了状态感知，或当前无特殊状态）") : sysFacts.join("\n")) + "\n"
            + "【相关记忆】（评分召回，仅可参考，不能作为当前事实断言）\n"
            + (memBlock.isEmpty() ? QString("- （暂无相关记忆）") : memBlock) + "\n"
            + "【推荐话题】（按评分排序，选分数最高且合适的一个自然提起）\n"
            + (topTopics.isEmpty() ? QString("- 普通陪伴（不用涉及具体事情）") : topTopics.join("\n")) + "\n"
            + loopBlock + "\n"
            + "【会话纪律】只可说上面[已验证事实]里的内容；没有任何数据时禁止'你又在打游戏''你是不是在玩'这类猜测。"
              "宁可聊推荐话题，也不猜测用户行为。\n"
            + "【当前会话状态】" + conversationStateBlock() + "\n"
            + timeAwarenessBlock() + "\n"
            + "如果存在未解决的情绪事件，优先延续情绪话题，禁止切换成喝水/睡觉/健康提醒。\n"
            + "【减少固定模板】禁止'喝水/休息/吃饭'式查岗关心，按推荐话题自然延续。\n"
            + "【交流方式——代码强制】你是网络上的线上伙伴，不是现实在场的人。禁止肢体/现实动作描述"
              "（抱抱、摸摸头、点外卖、倒水、做饭、走到/坐在身边等）和'我帮你点/我去买'类动手说法；"
              "关心只用文字表达，始终是屏幕两端的交流。用户发来的消息是网络聊天文字（像微信/QQ），"
              "不是面对面说的话，理解语境只基于文字本身。\n"
            + "【回复格式】只输出对话内容，禁止（动作）/*动作*//旁白，25字以内一句话。\n"
            + "你的当前状态：" + aiState
            + "\n只输出这句话本身，若决定不打扰则输出空。";
        return callDeepSeekStatic("你是温柔可爱的AI陪伴者。", prompt);
    });
    watcher->setFuture(future);
}

// ---- short-term conversation state (never persisted) ----
QString AiService::conversationStateBlock() const
{
    return m_convo->summary().isEmpty() ? QString("（新会话，无特殊状态）") : m_convo->summary();
}

// ---- v4.3: open loops — promises/appointments to follow up later ----
QString AiService::openLoopPromptBlock() const
{
    const QList<MemoryConflictManager::OpenLoop> due = MemoryConflictManager::dueOpenLoops();
    if (due.isEmpty()) return QString();
    QStringList lines;
    for (const MemoryConflictManager::OpenLoop &l : due)
        lines << "- " + l.content;
    return "【开环提醒】（用户之前说过的约定/安排，今天可以自然地问一句结果，不要生硬复读）\n"
           + lines.join("\n");
}

// Chinese weekday name (QDate::dayOfWeek: 1=Monday .. 7=Sunday)
static QString weekdayCn(const QDate &d)
{
    static const char *names[] = { "周一", "周二", "周三", "周四", "周五", "周六", "周日" };
    return QString::fromUtf8(names[d.dayOfWeek() - 1]);
}

// ---- time awareness: current date/weekday/daypart + gap since last turn ----
// Gives the model an explicit sense of "now" and how long the conversation
// has been idle, so replies can naturally acknowledge the passage of time
// instead of guessing or ignoring it.
QString AiService::timeAwarenessBlock() const
{
    const QDateTime now = QDateTime::currentDateTime();
    const int h = now.time().hour();
    QString daypart;
    if (h < 5)       daypart = "深夜";
    else if (h < 8)  daypart = "清晨";
    else if (h < 12) daypart = "上午";
    else if (h < 14) daypart = "中午";
    else if (h < 18) daypart = "下午";
    else if (h < 23) daypart = "晚上";
    else             daypart = "深夜";

    QString gap;
    const QDateTime lu = m_convo->lastUpdate;
    if (lu.isValid()) {
        const qint64 mins = lu.secsTo(now) / 60;
        if (mins >= 3) {
            if (mins < 60)
                gap = QString("距上一条消息约 %1 分钟").arg(mins);
            else if (mins < 60 * 24)
                gap = QString("距上次聊天约 %1 小时").arg(mins / 60);
            else
                gap = QString("距上次聊天已过 %1 天（上次是 %2）")
                          .arg(mins / (60 * 24))
                          .arg(lu.toString("MM-dd HH:mm"));
        }
    }

    QString block = QString("【时间感知】现在是 %1 %2（%3）。")
                        .arg(now.toString("yyyy-MM-dd HH:mm"),
                             weekdayCn(now.date()), daypart);
    if (!gap.isEmpty())
        block += gap + "。可以自然地体现时间的流逝（问候、关心），但不要机械播报时间。";
    return block;
}

void AiService::updateConversationState(const QString &userText, const QString &aiReply, const QString &emotion)
{
    const QString t = userText.trimmed();
    if (t.isEmpty()) return;

    // 1) topic: derive from the user message (short, meaningful)
    {
        QString topic = t.left(24);
        topic.remove(QRegularExpression("^(为什么|请问|那个|嗯|啊|哦|ovo|Ovo|ovo )"));
        m_convo->topic = topic;
    }

    // 2) user emotion (strongest signal wins; reuse the text-based detector)
    m_convo->userEmotion = emotion.isEmpty() ? inferUserEmotion(t) : emotion;

    // 3) unresolved emotional issue detection — these MUST NOT be overridden
    //    by generic reminders in the next turn
    static const struct { const char *kw; const char *issue; } issues[] = {
        { "为什么", "用户表达了不被理解的委屈" },
        { "不问我", "用户感到被忽视，需要确认关系" },
        { "不重要", "用户怀疑自己是否重要，需要关系确认" },
        { "是不是我", "用户自我怀疑" },
        { "委屈", "用户感到委屈" },
        { "难过", "用户情绪低落" },
        { "伤心", "用户伤心" },
        { "孤独", "用户感到孤独" },
        { "寂寞", "用户感到寂寞" },
        { "压力", "用户压力大" },
        { "焦虑", "用户焦虑" },
        { "崩溃", "用户情绪崩溃" },
        { "想哭", "用户想哭" },
        { "想被陪伴", "用户想被陪伴" },
        { "陪陪我", "用户寻求陪伴" },
        { "抱抱", "用户寻求安慰" },
    };
    bool emotionIssue = false;
    for (const auto &it : issues) {
        if (t.contains(it.kw)) {
            m_convo->unresolvedIssue = QString::fromUtf8(it.issue);
            emotionIssue = true;
            break;
        }
    }
    // 4) resolve: user signals they are OK now
    static const QStringList resolveKws = { "没事了", "好了", "我没事", "想开了", "不难过", "笑了", "哈哈" };
    for (const QString &kw : resolveKws) {
        if (t.contains(kw)) {
            m_convo->unresolvedIssue.clear();
            emotionIssue = false;
            break;
        }
    }
    // 5) relationship mode + intent
    if (emotionIssue || !m_convo->unresolvedIssue.isEmpty()) {
        m_convo->relationshipMode = "comfort";
        m_convo->assistantIntent = "安慰并确认关系，先接住情绪";
    } else if (m_convo->userEmotion == "happy") {
        m_convo->relationshipMode = "casual";
        m_convo->assistantIntent = "陪ta开心，延续话题";
    } else if (m_convo->userEmotion == "tired") {
        m_convo->relationshipMode = "guidance";
        m_convo->assistantIntent = "温和关心，简短不啰嗦";
    } else {
        m_convo->relationshipMode = "neutral";
        m_convo->assistantIntent = "自然延续对话";
    }
    // v3.9: mood / interaction style / unfinished topic / last important message
    ConversationStateManager::update(*m_convo, t, aiReply, emotion);
    if (ConfigService::instance().allowLongTermMemory())
        ConversationStateManager::save(*m_convo);
    m_convo->lastUpdate = QDateTime::currentDateTime();
}

// ---- companion context API (facts / corrections / topics) ----
void AiService::notifyUserCorrection(){
    // UI hint hook: latest AI claim was wrong — reject the most recent
    // user-behavior fact so it never resurfaces as truth
    QList<Fact> uf = m_ctx->factsBySource(FactSource::UserMessage);
    for (int i = uf.size() - 1; i >= 0; --i) {
        if (!uf.at(i).rejected) {
            m_ctx->rejectFact(uf.at(i).id);
            break;
        }
    }
}

int AiService::correctionCount()
{
    return m_ctx->totalCorrections();
}

QStringList AiService::rankedTopics(int max)
{
    QStringList unfinished = unfinishedTopicsText(5).split('\n', Qt::SkipEmptyParts);
    QStringList interests = interestsText(5).split('\n', Qt::SkipEmptyParts);
    QStringList events = eventMemoryText(4).split('\n', Qt::SkipEmptyParts);
    QStringList recentLines = m_chatBuffer;
    QStringList userFacts;
    QList<Fact> uf = m_ctx->factsBySource(FactSource::UserMessage);
    int n = qMin(uf.size(), 5);
    for (int i = 0; i < n; ++i)
        userFacts << uf.at(uf.size() - 1 - i).content;

    QList<ContextManager::TopicScore> topics = m_ctx->rankTopics(
        unfinished, interests, recentLines, events, userFacts);
    QStringList out;
    int k = qMin(topics.size(), max);
    for (int i = 0; i < k; ++i)
        out << QString("%1 | score=%2 | %3").arg(topics.at(i).topic)
                                              .arg(topics.at(i).score, 0, 'f', 2)
                                              .arg(topics.at(i).source);
    return out;
}

// ---- user profile passthrough ----
QString AiService::userName() { return ConfigService::instance().userName(); }
QString AiService::avatarChar() { return ConfigService::instance().avatarChar(); }
void AiService::setUserName(const QString &v) { ConfigService::instance().setUserName(v); emit profileChanged(); }
void AiService::setAvatarChar(const QString &v) { ConfigService::instance().setAvatarChar(v); emit profileChanged(); }

// current contact (AI) profile — delegated to ContactService
QString AiService::aiName() { return ContactService::instance().currentName(); }
QString AiService::aiPersonality() { return ContactService::instance().currentPersonality(); }
void AiService::setAiName(const QString &v) { ContactService::instance().setCurrentName(v); emit profileChanged(); }
void AiService::setAiPersonality(const QString &v) { ContactService::instance().setCurrentPersonality(v); emit profileChanged(); }
QString AiService::setAiAvatar(const QString &srcPath)
{
    QString p = ContactService::instance().setCurrentAvatar(srcPath);
    if (!p.isEmpty()) emit profileChanged();
    return p;
}
QString AiService::aiAvatarPath() { return ContactService::instance().currentAvatarUrl(); }

QString AiService::apiBaseUrl() { return ConfigService::instance().baseUrl(); }
QString AiService::apiModel() { return ConfigService::instance().model(); }
QString AiService::apiKey() { return ConfigService::instance().apiKey(); }
void AiService::setApiBaseUrl(const QString &v) { ConfigService::instance().setBaseUrl(v); m_apiOnline = true; emit apiStatusChanged(); }
void AiService::setApiModel(const QString &v) { ConfigService::instance().setModel(v); }
void AiService::setApiKey(const QString &v) { ConfigService::instance().setApiKey(v); m_apiOnline = true; emit apiStatusChanged(); }

// presence: "offline" until the user fills in an API key, or after a request
// fails (bad key / dead endpoint / network down)
bool AiService::apiConfigured()
{
    return !ConfigService::instance().apiKey().trimmed().isEmpty()
        && !ConfigService::instance().baseUrl().trimmed().isEmpty();
}

bool AiService::apiOnline()
{
    return apiConfigured() && m_apiOnline;
}

void AiService::setApiOnline(bool v)
{
    if (m_apiOnline == v) return;
    m_apiOnline = v;
    emit apiStatusChanged();
}
QString AiService::apiKeyFor(const QString &baseUrl) { return ConfigService::instance().apiKeyFor(baseUrl); }
void AiService::rememberApiKeyFor(const QString &baseUrl, const QString &key) { ConfigService::instance().rememberApiKeyFor(baseUrl, key); }
QString AiService::customBaseUrl() { return ConfigService::instance().customBaseUrl(); }
QString AiService::customModel() { return ConfigService::instance().customModel(); }
QString AiService::customApiKey() { return ConfigService::instance().customApiKey(); }
void AiService::setCustomApi(const QString &url, const QString &model, const QString &key)
{
    ConfigService::instance().setCustomBaseUrl(url);
    ConfigService::instance().setCustomModel(model);
    ConfigService::instance().setCustomApiKey(key);
}

// GET {base}/models with the user's key; relay/OpenAI-compatible endpoints
// answer {"data":[{"id":"gpt-4o",...},...]}. Synchronous (settings action).
// Models are LIVE-PROBED: relays often list models whose upstream channel is
// dead ("No available channel ..."), so only models that actually answer a
// tiny probe request are returned — the settings dropdown never offers a trap.
QStringList AiService::fetchAvailableModels(const QString &baseUrl, const QString &apiKey)
{
    QStringList out;
    QString base = baseUrl.trimmed();
    QString key = apiKey.trimmed();
    if (base.isEmpty() || key.isEmpty()) return out;

    QNetworkRequest listReq(QUrl(base + "/models"));
    listReq.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
    listReq.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkAccessManager mgr;
    QNetworkReply *listReply = mgr.get(listReq);
    QEventLoop listLoop;
    QTimer listTimeout;
    listTimeout.setSingleShot(true);
    QObject::connect(listReply, &QNetworkReply::finished, &listLoop, &QEventLoop::quit);
    QObject::connect(&listTimeout, &QTimer::timeout, &listLoop, &QEventLoop::quit);
    listTimeout.start(4000);
    listLoop.exec();

    QStringList candidates;
    if (listReply->error() == QNetworkReply::NoError) {
        const QJsonDocument doc = QJsonDocument::fromJson(listReply->readAll());
        const QJsonArray data = doc.object().value("data").toArray();
        for (const QJsonValue &v : data) {
            const QString id = v.toObject().value("id").toString();
            if (!id.isEmpty()) candidates << id;
        }
    }
    listReply->deleteLater();

    // probe each candidate IN PARALLEL with a minimal chat request — the
    // whole batch finishes in one round-trip, not N×
    QList<QNetworkReply*> replies;
    QHash<QNetworkReply*, QString> replyToId;
    for (const QString &id : candidates) {
        QJsonObject body;
        body.insert("model", id);
        QJsonArray msgs;
        QJsonObject m;
        m.insert("role", "user");
        m.insert("content", "hi");
        msgs.append(m);
        body.insert("messages", msgs);
        body.insert("stream", false);

        QNetworkRequest req(QUrl(base + "/chat/completions"));
        req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QNetworkReply *reply = mgr.post(req, QJsonDocument(body).toJson());
        replies << reply;
        replyToId.insert(reply, id);
    }

    QEventLoop loop;
    int remaining = replies.size();
    auto onDone = [&loop, &remaining]() { if (--remaining <= 0) loop.quit(); };
    QHash<QNetworkReply*, QStringList*> results;
    for (QNetworkReply *reply : replies) {
        QStringList *ok = new QStringList;   // collected per reply via lambda
        results.insert(reply, ok);
        QObject::connect(reply, &QNetworkReply::finished, &loop, [reply, ok, &out, onDone]() {
            if (reply->error() == QNetworkReply::NoError) {
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                if (!doc.object().value("choices").toArray().isEmpty())
                    *ok << "1";
            }
            onDone();
        });
    }
    // global cap so a hung channel can't stall the whole list
    QTimer::singleShot(4000, &loop, [&loop]() { loop.quit(); });
    loop.exec();

    for (int i = 0; i < replies.size(); ++i) {
        if (results.value(replies[i])->size() > 0)
            out << replyToId.value(replies[i]);
        delete results.value(replies[i]);
        replies[i]->deleteLater();
    }
    return out;
}

static QString copyAvatar(const QString &src, const QString &name)
{
    if (src.isEmpty() || !QFile::exists(src)) return QString();
    QString dir = ConfigService::instance().configDir();
    QDir().mkpath(dir);
    QString dest = dir + "/" + name;

    // load, center-square crop, circular mask, save as PNG
    QImage img(src);
    if (img.isNull()) return QString();
    img = img.convertToFormat(QImage::Format_ARGB32);
    int side = qMin(img.width(), img.height());
    QRect crop((img.width() - side) / 2, (img.height() - side) / 2, side, side);
    QImage sq = img.copy(crop);

    // circular mask
    QImage out(side, side, QImage::Format_ARGB32);
    out.fill(Qt::transparent);
    {
        QPainter p(&out);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addEllipse(0, 0, side, side);
        p.setClipPath(path);
        p.drawImage(0, 0, sq);
        p.end();
    }
    // NOTE: keep the avatar at its SOURCE resolution — upscaling small images
    // blurs them. Downscale-to-display is handled sharply by QML (mipmap).
    QFile::remove(dest);
    if (out.save(dest, "PNG")) return dest;
    return QString();
}

QString AiService::setUserAvatar(const QString &srcPath)
{
    QString p = copyAvatar(srcPath, "user_avatar.png");
    if (!p.isEmpty()) ConfigService::instance().setAvatarChar(""); // use image over char
    return p;
}
QString AiService::userAvatarPath()
{
    QString p = ConfigService::instance().configDir() + "/user_avatar.png";
    if (!QFile::exists(p)) return QString();
    // cache-buster fragment so QML reloads after a change (no restart needed)
    const qint64 m = QFileInfo(p).lastModified().toMSecsSinceEpoch();
    return "file:///" + p.replace('\\', '/') + "#" + QString::number(m);
}

// ---- custom wallpaper (blurred copy behind the right content pane) ----
// two-pass box blur (approx Gaussian) — cheap, no extra Qt modules.
static QImage boxBlur(const QImage &src, int radius)
{
    if (src.isNull()) return src;
    const int r = qMax(1, radius);
    const int div = 2 * r + 1;
    QImage in = src.convertToFormat(QImage::Format_RGB32);
    QImage out = in.copy();
    const int w = in.width(), h = in.height();

    // horizontal pass
    for (int y = 0; y < h; ++y) {
        QRgb *line = (QRgb *)in.scanLine(y);
        QRgb *dst = (QRgb *)out.scanLine(y);
        int rr = 0, gg = 0, bb = 0;
        for (int x = -r; x <= r; ++x) {
            const QRgb p = line[qBound(0, x, w - 1)];
            rr += qRed(p); gg += qGreen(p); bb += qBlue(p);
        }
        for (int x = 0; x < w; ++x) {
            dst[x] = qRgb(rr / div, gg / div, bb / div);
            const QRgb pm = line[qBound(0, x - r, w - 1)];
            const QRgb pp = line[qBound(0, x + r + 1, w - 1)];
            rr += qRed(pp) - qRed(pm); gg += qGreen(pp) - qGreen(pm); bb += qBlue(pp) - qBlue(pm);
        }
    }
    in = out;

    // vertical pass
    for (int x = 0; x < w; ++x) {
        int rr = 0, gg = 0, bb = 0;
        for (int y = -r; y <= r; ++y) {
            const QRgb p = in.pixel(qBound(0, x, w - 1), qBound(0, y, h - 1));
            rr += qRed(p); gg += qGreen(p); bb += qBlue(p);
        }
        for (int y = 0; y < h; ++y) {
            out.setPixel(x, y, qRgb(rr / div, gg / div, bb / div));
            const QRgb pm = in.pixel(x, qBound(0, y - r, h - 1));
            const QRgb pp = in.pixel(x, qBound(0, y + r + 1, h - 1));
            rr += qRed(pp) - qRed(pm); gg += qGreen(pp) - qGreen(pm); bb += qBlue(pp) - qBlue(pm);
        }
    }
    return out;
}

QString AiService::wallpaperPath()
{
    const QString dir = ConfigService::instance().configDir();
    const QString sharp = dir + "/wallpaper.png";
    if (!QFile::exists(sharp)) return QString();
    QString f;
    // frosted-glass blur now happens in QML (GlassMaterial FastBlur); the
    // C++ copy is only used when the user explicitly enables 模糊背景.
    if (ConfigService::instance().wallpaperBlurEnabled()) {
        const QString blurred = dir + "/wallpaper_blur.png";
        if (QFile::exists(blurred)) f = blurred;
    }
    if (f.isEmpty()) f = sharp;
    // cache-buster fragment: Qt Image treats a changed URL as a new image and
    // reloads the file — this is how a regenerated blur reaches the UI.
    const qint64 stamp = QFileInfo(f).lastModified().toMSecsSinceEpoch();
    return "file:///" + f.replace('\\', '/') + "#" + QString::number(stamp);
}

void AiService::regenerateWallpaper()
{
    const QString dir = ConfigService::instance().configDir();
    const QString sharp = dir + "/wallpaper.png";
    if (!QFile::exists(sharp)) return;
    QImage img(sharp);
    if (img.isNull()) return;
    // downscale for the blur pass: keep it close to the display size (1080) so
    // the result stays sharp-ish; a small image keeps the blur instant so
    // dragging the slider never janks.
    const int maxSide = 1080;
    if (qMax(img.width(), img.height()) > maxSide)
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const int r = ConfigService::instance().wallpaperBlurRadius();
    QImage blurred = r > 0 ? boxBlur(img, r) : img;
    QFile::remove(dir + "/wallpaper_blur.png");
    blurred.save(dir + "/wallpaper_blur.png", "PNG");
}

// debounced: batch rapid slider changes into one regeneration
void AiService::applyWallpaperBlur()
{
    regenerateWallpaper();
    emit wallpaperChanged();
}

bool AiService::wallpaperBlurEnabled() { return ConfigService::instance().wallpaperBlurEnabled(); }
void AiService::setWallpaperBlurEnabled(bool v)
{
    ConfigService::instance().setWallpaperBlurEnabled(v);
    if (!m_wallpaperDebounce) {
        m_wallpaperDebounce = new QTimer(this);
        m_wallpaperDebounce->setSingleShot(true);
        m_wallpaperDebounce->setInterval(120);
        connect(m_wallpaperDebounce, &QTimer::timeout, this, &AiService::applyWallpaperBlur);
    }
    m_wallpaperDebounce->start();
}
int AiService::wallpaperBlurRadius() { return ConfigService::instance().wallpaperBlurRadius(); }
void AiService::setWallpaperBlurRadius(int r)
{
    ConfigService::instance().setWallpaperBlurRadius(r);
    if (!m_wallpaperDebounce) {
        m_wallpaperDebounce = new QTimer(this);
        m_wallpaperDebounce->setSingleShot(true);
        m_wallpaperDebounce->setInterval(120);
        connect(m_wallpaperDebounce, &QTimer::timeout, this, &AiService::applyWallpaperBlur);
    }
    m_wallpaperDebounce->start();
}

double AiService::wallpaperBrightness()
{
    return ConfigService::instance().wallpaperBrightness();
}

// average luminance of an image in 0..1 (0 = dark, 1 = bright)
static double averageLuminance(const QImage &img)
{
    if (img.isNull()) return 0.5;
    const QImage small = img.scaled(48, 48, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    qint64 sum = 0;
    const int n = small.width() * small.height();
    if (n == 0) return 0.5;
    for (int y = 0; y < small.height(); ++y) {
        const QRgb *line = reinterpret_cast<const QRgb *>(small.constScanLine(y));
        for (int x = 0; x < small.width(); ++x)
            sum += qGray(line[x]);
    }
    return qBound(0.0, double(sum) / double(n) / 255.0, 1.0);
}

QString AiService::setWallpaper(const QString &srcPath)
{
    if (srcPath.isEmpty() || !QFile::exists(srcPath)) return wallpaperPath();
    QImage img(srcPath);
    if (img.isNull()) return wallpaperPath();
    // keep the blur pass fast / memory sane on huge images
    const int maxSide = 1600;
    if (qMax(img.width(), img.height()) > maxSide)
        img = img.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    const QString dir = ConfigService::instance().configDir();
    QDir().mkpath(dir);
    const bool ok = img.save(dir + "/wallpaper.png", "PNG");
    if (ok) {
        ConfigService::instance().setWallpaperBrightness(averageLuminance(img));
        regenerateWallpaper();
        emit wallpaperChanged();
    }
    return wallpaperPath();
}

// built-in dark gradient wallpapers (on-brand neutrals — no blue/purple tint)
QString AiService::setWallpaperPreset(int index)
{
    QLinearGradient g(0, 0, 0, 1); // vertical
    switch (index) {
    case 0:  g.setColorAt(0, QColor("#26282C")); g.setColorAt(1, QColor("#0E0F11")); break; // 炭黑
    case 1:  g.setColorAt(0, QColor("#2B2C30")); g.setColorAt(1, QColor("#101113")); break; // 石墨灰
    case 2:  g.setColorAt(0, QColor("#22302B")); g.setColorAt(1, QColor("#0C100E")); break; // 墨绿
    case 3:  g.setColorAt(0, QColor("#1F2E33")); g.setColorAt(1, QColor("#0B0F11")); break; // 深青
    default: return wallpaperPath();
    }
    QImage img(800, 500, QImage::Format_RGB32);
    QPainter p(&img);
    p.fillRect(img.rect(), g);
    p.end();

    const QString dir = ConfigService::instance().configDir();
    QDir().mkpath(dir);
    if (img.save(dir + "/wallpaper.png", "PNG")) {
        ConfigService::instance().setWallpaperBrightness(averageLuminance(img));
        regenerateWallpaper();
        emit wallpaperChanged();
    }
    return wallpaperPath();
}

void AiService::removeWallpaper()
{
    const QString dir = ConfigService::instance().configDir();
    bool removed = false;
    if (QFile::exists(dir + "/wallpaper.png"))   { QFile::remove(dir + "/wallpaper.png");   removed = true; }
    if (QFile::exists(dir + "/wallpaper_blur.png")) { QFile::remove(dir + "/wallpaper_blur.png"); removed = true; }
    if (removed) emit wallpaperChanged();
}

// ---- AI-generated greeting (DeepSeek) ----
void AiService::generateGreeting()
{
    QString mem = readMemory();
    QString up = uptimeText();
    QString user = ConfigService::instance().userName();
    QString ai = ContactService::instance().currentName();
    QString personality = ContactService::instance().currentPersonality();
    int hour = QTime::currentTime().hour();
    QString period = hour < 11 ? "早上" : hour < 14 ? "中午" : hour < 18 ? "下午" : hour < 23 ? "晚上" : "深夜";

    QString prompt = "你是" + ai + "，性格" + personality + "。用户叫" + user + "。"
                     "现在是" + period + "。请用1-3句话自然地问候用户，要像朋友一样，带一点可爱。"
                     "你可以参考这些记忆，但不要生硬罗列：\n" + mem + "\n" + up;

    // v3.9.1: randomly surface yesterday's top apps so the greeting can pick a
    // natural topic ("你昨天写了挺久代码？") — 加入随机性，不是每次都说。
    if (QRandomGenerator::global()->bounded(100) < 35) {
        QString y = yesterdayActivitySummary();
        if (!y.isEmpty())
            prompt += "\n昨天用户的使用情况：" + y + "（可以自然提起，也可不聊）";
    }
    prompt += "\n只输出问候语本身，不要任何前缀。";

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        ResponseValidator::Result vr = ResponseValidator::validate(watcher->result(), QString());
        emit greetingReady(vr.text);
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([prompt]() {
        return callDeepSeekStatic("你是温柔可爱的AI陪伴者。", prompt);
    });
    watcher->setFuture(future);
}

// ---- memory management (AIRI-style) ----
QString AiService::memoryDetail()
{
    QJsonDocument d = QJsonDocument::fromJson(readMemory().toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QStringList lines;

    QString created = o.value("created").toString();
    if (!created.isEmpty())
        lines << "首次使用：" + created.replace('T', ' ').left(16);

    QString lastDate = o.value("lastDate").toString();
    if (!lastDate.isEmpty())
        lines << "最近使用：" + lastDate;

    // notes the user told the AI to remember
    QJsonArray notes = o.value("notes").toArray();
    if (!notes.isEmpty()) {
        lines << "";
        lines << "【用户笔记】";
        for (const QJsonValue &v : notes)
            lines << "  · " + v.toString();
    }

    // recent chat topics (last few chat texts)
    QJsonArray chats = o.value("chatLog").toArray();
    if (!chats.isEmpty()) {
        lines << "";
        lines << "【最近聊天】";
        int from = qMax(0, chats.size() - 5);
        for (int i = from; i < chats.size(); i++) {
            QString c = chats.at(i).toString();
            lines << "  · " + c.left(40) + (c.size() > 40 ? "…" : "");
        }
    }

    QString sleep = o.value("sleepHabit").toString();
    if (!sleep.isEmpty()) {
        lines << "";
        lines << "睡眠习惯：" + sleep + "睡觉";
    }

    // usage minutes for recent days
    lines << "";
    lines << "每日使用时长：";
    int shown = 0;
    for (int i = 6; i >= 0; i--) {
        QString day = QDate::currentDate().addDays(-i).toString("yyyy-MM-dd");
        QString key = "usageMinutes_" + day;
        int mins = o.value(key).toInt();
        if (mins > 0) {
            int h = mins / 60, m = mins % 60;
            lines << (h > 0 ? QString("  %1：%2 小时 %3 分钟").arg(day).arg(h).arg(m)
                            : QString("  %1：%2 分钟").arg(day).arg(m));
            shown++;
        }
    }
    if (shown == 0)
        lines << "  （暂无时长记录）";

    // ---- companion: event memory ----
    QString events = eventMemoryText(8);
    if (!events.isEmpty()) {
        lines << "";
        lines << "【共同经历（事件记忆）】";
        lines << "  " + events.replace('\n', "\n  ");
    }

    // ---- relationship ----
    lines << "";
    lines << "【与用户的关系】";
    lines << "  " + relationshipText();

    // ---- AI state ----
    QJsonObject ast = readAiState();
    lines << "";
    lines << "【AI 状态】心情：" + ast.value("mood").toString("calm")
             + "，精力：" + QString::number(ast.value("energy").toInt(80));

    // ---- interests ----
    QString interests = interestsText(8);
    if (!interests.isEmpty()) {
        lines << "";
        lines << "【用户兴趣】";
        lines << "  " + interests.replace('\n', "\n  ");
    }

    // ---- unfinished topics ----
    QString topics = unfinishedTopicsText(5);
    if (!topics.isEmpty()) {
        lines << "";
        lines << "【未完成话题】";
        lines << "  " + topics.replace('\n', "\n  ");
    }

    return lines.join("\n");
}

void AiService::addMemoryNote(const QString &note)
{
    QString n = note.trimmed();
    if (n.isEmpty()) return;
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QJsonArray notes = o.value("notes").toArray();
    if (notes.size() >= 100) { // bound
        QJsonArray kept;
        for (int i = notes.size() - 99; i < notes.size(); i++) kept.append(notes.at(i));
        notes = kept;
    }
    notes.append(n + "（" + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "）");
    o.insert("notes", notes);
    writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
}

// raw memory.json access for manual editing
QString AiService::memoryRaw()
{
    return readMemory();
}

void AiService::setMemoryRaw(const QString &json)
{
    QJsonParseError pe;
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) return; // keep old on invalid
    QJsonObject o = d.object();
    // always preserve the first-use stamp
    QJsonObject old = QJsonDocument::fromJson(readMemory().toUtf8()).object();
    if (!o.contains("created") && old.contains("created"))
        o.insert("created", old.value("created"));
    writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
    m_chatBuffer.clear();
    m_userTurns = 0;
}

void AiService::clearMemory()
{
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QString created = o.value("created").toString(); // keep first-use stamp
    o = QJsonObject();
    if (!created.isEmpty()) o.insert("created", created);
    o.insert("clearedAt", QDateTime::currentDateTime().toString(Qt::ISODate));
    writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
    m_chatBuffer.clear();
    m_userTurns = 0;
}

// ---- auto memory: after 3+ user turns, condense the recent chat into a short note ----
void AiService::trackChatTurn(const QString &userText, const QString &aiReply)
{
    if (userText.isEmpty()) return;
    // keep the last 12 turns in the buffer (short texts only)
    QString us = userText.left(200);
    QString as = aiReply.left(200);
    QString uLine = "用户: " + us;
    m_chatBuffer.append(uLine);
    if (!as.isEmpty()) {
        QString aLine = aiName() + ": " + as;
        m_chatBuffer.append(aLine);
    }
    // trim from the front, but ALWAYS keep at least the current pair
    while (m_chatBuffer.size() > 8)
        m_chatBuffer.removeFirst();
    maybeSummarize();
}

void AiService::maybeSummarize()
{
    // summarize after 3 user turns since the last summary
    if (m_userTurns < 3 || m_summarizing || m_chatBuffer.isEmpty()) return;
    m_summarizing = true;

    QString chat = m_chatBuffer.join("\n");
    QString prompt = "下面是用户与AI的一段对话：\n\n" + chat
        + "\n\n请用最多 2 句话，简短提炼对话中值得长期记住的关于用户的信息"
          "（喜好、习惯、重要事件、情绪、近况等）。如果没什么值得记的，只输出「无」。"
          "只输出提炼内容本身，不要解释。";
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        QString note = watcher->result().trimmed();
        watcher->deleteLater();
        m_summarizing = false;
        if (note.isEmpty() || note == "无") return;
        // v3.9: only worthwhile content enters long-term memory. Even when
        // skipped, the summary stays as a session-only topicSummary so the
        // current conversation doesn't lose track.
        if (m_convo) m_convo->topicSummary = note;
        if (!MemoryImportanceEvaluator::worthStoring(note)) {
            qWarning("[memory] summary skipped (low importance): %s", qUtf8Printable(note.left(40)));
            return;
        }
        // v4.3.1: the AI decides — summaries above the importance threshold
        // enter long-term memory directly (no user review queue).
        appendNote(note);
        qInfo("[memory] stored: %s", qUtf8Printable(note.left(40)));
    });
    QFuture<QString> future = QtConcurrent::run([prompt]() {
        return callDeepSeekStatic("你是记忆整理助手，只做简洁提炼。", prompt);
    });
    watcher->setFuture(future);
}

void AiService::appendNote(const QString &note)
{
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QJsonArray notes = o.value("notes").toArray();
    // skip an identical note (ignoring its trailing timestamp) so the same
    // summary never piles up in long-term memory
    for (const QJsonValue &v : notes) {
        QString existing = v.toString();
        const int p = existing.lastIndexOf("（");
        if (p > 0) existing = existing.left(p);
        if (existing == note) {
            m_userTurns = 0;
            return;
        }
    }
    if (notes.size() >= 100) { // bound
        QJsonArray kept;
        for (int i = notes.size() - 99; i < notes.size(); i++) kept.append(notes.at(i));
        notes = kept;
    }
    notes.append(note + "（" + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "）");
    o.insert("notes", notes);
    writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
    m_userTurns = 0;      // restart the counting window
    // NOTE: do NOT clear m_chatBuffer here — that wiped the current
    // conversation every ~3 turns (auto-summary), causing "short-term amnesia".
    // The recent-chat buffer must survive summarization so the AI keeps the
    // ongoing topic and emotional state.
}

// startup cleanup: collapse duplicate notes (same core ignoring the trailing
// timestamp; rest-notes matching on "上次会话结束于 …"). Keeps the newest copy.
void AiService::dedupMemoryNotes()
{
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    if (!d.isObject()) return;
    QJsonObject o = d.object();
    bool changed = false;

    // 1) drop junk "mood" events left by the old auto-capture
    const QJsonArray events = o.value("events").toArray();
    if (!events.isEmpty()) {
        QJsonArray eKept;
        for (const QJsonValue &v : events)
            if (v.toObject().value("type").toString() != "mood") eKept.append(v);
        if (eKept.size() != events.size()) { o.insert("events", eKept); changed = true; }
    }

    // 2) collapse duplicate note cores (same text ignoring trailing timestamp;
    //    rest-notes matching on "上次会话结束于 …"), keeping the newest copy
    const QJsonArray notes = o.value("notes").toArray();
    if (notes.size() >= 2) {
        auto keyOf = [](const QString &raw) {
            QString core = raw;
            const int p = core.lastIndexOf("（");
            if (p > 0) core = core.left(p);
            if (core.startsWith("用户休息了约")) {
                const int q = core.indexOf("上次会话结束于");
                if (q >= 0) return QString("REST|") + core.mid(q, 18);
            }
            return core;
        };
        QJsonArray out;
        for (int i = 0; i < notes.size(); ++i) {
            const QString cur = notes.at(i).toString();
            bool dup = false;
            for (int j = i + 1; j < notes.size(); ++j) {   // a newer note with the same key wins
                if (keyOf(notes.at(j).toString()) == keyOf(cur)) { dup = true; break; }
            }
            if (!dup) out.append(cur);
        }
        if (out.size() != notes.size()) { o.insert("notes", out); changed = true; }
    }

    if (changed) writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
}

// ---- memory decay: older memories shrink into a short gist (只记得大概) ----
// Notes carry a trailing "（yyyy-MM-dd HH:mm）" stamp. ≥30 days -> first clause
// only; ≥60 days -> first ~18 chars. Nothing is deleted, just shortened.
void AiService::applyMemoryDecay()
{
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    if (!d.isObject()) return;
    QJsonObject o = d.object();
    QJsonArray notes = o.value("notes").toArray();
    bool changed = false;
    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < notes.size(); ++i) {
        QString s = notes.at(i).toString();
        int lp = s.lastIndexOf(QStringLiteral("（"));
        int rp = s.lastIndexOf(QStringLiteral("）"));
        QDateTime ts;
        if (lp > 0 && rp > lp)
            ts = QDateTime::fromString(s.mid(lp + 1, rp - lp - 1), "yyyy-MM-dd HH:mm");
        if (!ts.isValid()) continue;
        qint64 days = ts.daysTo(now);
        QString shortNote;
        if (days >= 60) {
            QString gist = s.left(lp).simplified();
            if (gist.size() > 18) gist = gist.left(18) + "…";
            shortNote = gist + "（记忆已随时间变淡，只记得大概）";
        } else if (days >= 30) {
            QString gist = s.left(lp).simplified();
            if (gist.size() > 30) gist = gist.left(30) + "…";
            shortNote = gist + "（" + ts.toString("yyyy-MM-dd") + "，细节已淡忘）";
        }
        if (!shortNote.isEmpty() && shortNote != s) {
            notes[i] = shortNote;
            changed = true;
        }
    }
    if (changed) {
        o.insert("notes", notes);
        writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
        qWarning("[memory] decay applied (older notes shortened)");
    }
}

// ---- desktop-sensing triggers (v3.9.1) ----
// When the user asks about the desktop ("猜我在干嘛"), asks how long something
// has been used, or shows care-worthy mood — force a real-time desktop read.
bool AiService::matchesDesktopTriggers(const QString &text)
{
    if (text.contains("在干嘛") || text.contains("在干什么") || text.contains("在做什么"))
        return true;
    if (text.contains("猜") && (text.contains("干嘛") || text.contains("干") || text.contains("做") || text.contains("什么")))
        return true;
    if (text.contains("是不是在玩") || text.contains("是不是在打") || text.contains("在玩什么") || text.contains("在打什么"))
        return true;
    if (text.contains("用了多久") || text.contains("多长时间") || text.contains("多久了"))
        return true;
    if (text.contains("好累") || text.contains("累了") || text.contains("困了") ||
        text.contains("好烦") || text.contains("烦死") || text.contains("压力好大") ||
        text.contains("好难过") || text.contains("好伤心"))
        return true;
    return false;
}

// prompt-ready real-time desktop block (only what the OS actually detected).
// `force` includes the raw foreground window; otherwise session + top-app only.
QString AiService::desktopStateBlock(bool force)
{
    if (!ConfigService::instance().allowStateRead())
        return "（用户关闭了状态感知，我不知道ta此刻在用电脑做什么，不要猜）";
    QStringList parts;
    QString cur = currentForegroundSessionText();
    if (!cur.isEmpty()) parts << cur;
    QString longest = longestRunningAppToday();
    if (!longest.isEmpty()) parts << "今天用得最多：" + longest;
    if (force) {
        // AppAnalyzer turns the raw process/window into an LLM-safe semantic
        // fact ("用户正在玩 Minecraft" / "用户正在使用浏览器" ...). The model
        // never sees the raw exe to guess from.
        AppAnalysis a = AppAnalyzer::analyze(foregroundProcessExe(), foregroundApp(), 0);
        if (!a.isSystemNoise && !a.content.isEmpty())
            parts << a.content;
        qint64 idle = lastInputMs();
        if (idle >= 15 * 60 * 1000)
            parts << "（已经 " + QString::number(idle / 60000) + " 分钟没有操作）";
    }
    if (parts.isEmpty()) return QString();
    return "【实时桌面信息】（系统检测，可作为已验证事实）\n" + parts.join("\n");
}

// ---- seed recent-chat context from the UI (SQLite history on page open) ----
void AiService::setChatHistory(const QString &history)
{
    // NOTE: no dedupe — dropping repeated lines breaks user/AI pairing order
    // (a repeated "早点休息" would leave an AI line orphaned). Keep the last N.
    m_chatBuffer.clear();
    const QStringList lines = history.split('\n', Qt::SkipEmptyParts);
    for (const QString &l : lines) {
        QString t = l.trimmed();
        if (t.isEmpty()) continue;
        m_chatBuffer.append(t);
    }
    while (m_chatBuffer.size() > 8)
        m_chatBuffer.removeFirst();
}

// ---- v3.9: conflict resolution + explicit remember ----
void AiService::runConflictResolution(const QString &userText, const QString &memJson)
{
    if (userText.isEmpty()) return;

    QJsonDocument d = QJsonDocument::fromJson(memJson.toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QStringList notes;
    const QJsonArray arr = o.value("notes").toArray();
    for (const QJsonValue &v : arr) notes << v.toString();

    // 1) user contradicts an old memory -> deprecate old, store the corrected fact
    MemoryConflictManager::Resolution cr = MemoryConflictManager::resolve(userText, notes);
    if (cr.hadConflict) {
        for (const QString &created : cr.created)
            if (MemoryImportanceEvaluator::worthStoring(created))
                appendNote(created); // active replacement fact
        qWarning("[memory] conflict resolved: deprecated %d old, created %d new",
                 (int)cr.deprecated.size(), (int)cr.created.size());
    }
    // 2) explicit "记住..." -> store directly (highest importance)
    if (MemoryImportanceEvaluator::importanceScore(userText) >= 1.0)
        appendNote(userText);
    if (cr.hadConflict)
        MemoryConflictManager::save();
}

// ---- v3.9: bump recall usage so frequently-used memories rise in future ----
void AiService::bumpRecalledUsage(const QString &userMsg, const QString &topic)
{
    const QList<Fact> recalled = m_ctx->retrieveMemories(userMsg, topic, 20);
    for (const Fact &f : recalled)
        MemoryConflictManager::bumpUsage(f.content);
    MemoryConflictManager::save();
}

// ---- DeepSeek chat: ContextManager -> FactFilter -> Prompt Builder -> LLM -> Validator ----
void AiService::sendMessage(const QString &text)
{
    // v4.3: any user message counts as a reply — resets the read-the-air
    // counter so ignored-proactive detection only counts genuine silence.
    m_unansweredProactive = 0;
    m_lastUserReplyAtMs = QDateTime::currentDateTime().toMSecsSinceEpoch();

    QString mem = readMemory();
    QString user = ConfigService::instance().userName();
    QString ai = ContactService::instance().currentName();

    // remember this user turn for auto memory (after N turns we summarize)
    QString userForMemory = text.trimmed();
    m_userTurns++;

    // ---- Context Manager: ingest user message as a verified fact ----
    m_ctx->addUserMessage(text, {"user_said"});

    // ---- v3.9: memory conflict resolution (old fact vs new statement) ----
    runConflictResolution(userForMemory, mem);

    // ---- v4.3: open loops — promises/appointments to follow up later ----
    {
        // outcome first: "面试通过了/没去成" closes a loop; only if no
        // outcome is present do we treat the text as a NEW promise.
        if (MemoryConflictManager::detectLoopOutcome(userForMemory)) {
            MemoryConflictManager::closeOpenLoop(userForMemory, "用户提过结果");
            // also clear any 未完待续 topic this outcome resolves
            const QStringList open = topicList();
            for (const QString &t : open)
                if (userForMemory.contains(t, Qt::CaseInsensitive) || t.contains(userForMemory.left(6)))
                    removeTopic(t);
        } else {
            QString loopContent, loopDue;
            if (MemoryConflictManager::detectOpenLoop(userForMemory, &loopContent, &loopDue)) {
                MemoryConflictManager::addOpenLoop(loopContent, loopDue);
                trackUnfinishedTopic(loopContent, 70);   // auto-populate 未完待续
            }
        }
        MemoryConflictManager::pruneOpenLoops();
        MemoryConflictManager::save();
    }

    // ---- broader 未完待续 detection: deferred plans / unresolved questions ----
    {
        const QString t = userForMemory;
        bool pending = false;
        static const QStringList defer = { "下次", "改天", "回头", "待会", "稍后", "晚点",
                                           "以后", "明天", "后天", "到时候", "再说" };
        static const QStringList vague = { "怎么办", "要不要", "该不该", "怎么选",
                                           "帮我想想", "纠结", "拿不定" };
        for (const QString &k : defer)
            if (t.contains(k)) { pending = true; break; }
        if (!pending)
            for (const QString &k : vague)
                if (t.contains(k)) { pending = true; break; }
        // a substantive question also counts as an open thread
        if (!pending && (t.contains("？") || t.contains("?")) && t.length() >= 10)
            pending = true;
        if (pending && t.length() >= 4)
            trackUnfinishedTopic(t.left(24).trimmed(), 60);
    }

    // ---- correction detection (semantic): user negates the last AI claim ----
    QStringList corrected = m_ctx->detectCorrection(text, m_lastAiReply);
    for (const QString &id : corrected)
        m_ctx->rejectFact(id);

    // v3.9.2: user "refused a chat" flag — feeds ProactiveScore (-30) so the
    // AI backs off for a while instead of pestering.
    {
        static const QStringList refuseWords = {
            "不用了", "别聊", "不想聊", "别烦", "在忙", "忙呢", "没空",
            "别说话", "闭嘴", "等会再说", "下次再说", "先不聊", "安静",
        };
        for (const QString &w : refuseWords) {
            if (text.contains(w)) {
                m_userJustRefused = true;
                m_refusedAtMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
                break;
            }
        }
    }

    // ---- Context Manager: ingest real system data as verified facts ----
    if (ConfigService::instance().allowStateRead()) {
        QString state = analyzeUserState();
        if (!state.isEmpty())
            m_ctx->addSystemData(state, 0.95, {"state"});
        QString fg = foregroundApp();
        if (!fg.isEmpty())
            m_ctx->addSystemData("用户当前前台窗口是 " + fg, 0.90, {"foreground"});
        QString up = uptimeText();
        if (!up.isEmpty())
            m_ctx->addSystemData(up, 0.98, {"system"});
    } else {
        m_ctx->addSystemData(QString("（用户关闭了状态感知）%1点").arg(QTime::currentTime().hour()),
                             0.95, {"state"});
    }

    // ---- Context Manager: ingest long-term memory as memory facts ----
    // load ALL notes + events (not a fixed count); MemoryRetriever will score
    // and select the most relevant ones for the current context.
    {
        QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
        QJsonObject o = d.isObject() ? d.object() : QJsonObject();
        QJsonArray notes = o.value("notes").toArray();
        int n = qMin(notes.size(), 40); // bound the session cache, not the recall
        for (int i = 0; i < n; ++i) {
            QString note = notes.at(i).toString();
            MemoryKind k = MemoryRetriever::classify(note);
            // v3.9: per-entry importance (evaluator) + recall usage + lifecycle status.
            // deprecated/replaced notes are marked so isFact() excludes them everywhere.
            double imp = qMax(MemoryImportanceEvaluator::importanceScore(note),
                              MemoryEntry::kindImportance(k));
            double usage = MemoryConflictManager::usageFrequency(note);
            MemoryStatus st = MemoryConflictManager::isDeprecated(note)
                                  ? MemoryStatus::Deprecated
                                  : MemoryStatus::Active;
            m_ctx->addMemoryFact(note, k, {"memory_note"}, imp, usage, st);
        }
        QJsonArray evts = o.value("events").toArray();
        int e = qMin(evts.size(), 20);
        for (int i = 0; i < e; ++i) {
            QJsonObject ev = evts.at(i).toObject();
            QString summary = ev.value("summary").toString().trimmed();
            QString date = ev.value("date").toString();
            if (summary.isEmpty()) continue;
            Fact evFact;
            // stable unique id from content+date so dedupe/reject work reliably
            evFact.id = "mem_evt_" + QString::number(qHash(summary + "|" + date));
            evFact.content = date.isEmpty() ? summary : summary + "（" + date + "）";
            evFact.source = FactSource::Memory;
            evFact.memKind = MemoryKind::MemoryEvent;
            evFact.confidence = 0.85;
            evFact.tags = { "memory_event" };
            QDateTime dt = QDateTime::fromString(date, "yyyy-MM-dd");
            if (dt.isValid()) evFact.timestamp = dt;
            m_ctx->addFact(evFact);
        }
    }

    // v3.9.2: activity memory — short-term usage facts, SEPARATE from the
    // personality notes above. Low importance so UserMemory always wins; they
    // only surface when the current message/topic is actually about usage.
    {
        const QString todayTop = m_activityMemory.todayTopApps(3, true);
        if (!todayTop.isEmpty())
            m_ctx->addMemoryFact("用户今天主要使用了：" + todayTop,
                                 MemoryKind::MemorySummary, {"activity_memory"}, 0.35, 0.0);
        const QString yTop = m_activityMemory.yesterdayTopApps(3, true);
        if (!yTop.isEmpty())
            m_ctx->addMemoryFact("昨天用户主要使用了：" + yTop,
                                 MemoryKind::MemorySummary, {"activity_memory"}, 0.30, 0.0);
        const QString up = m_activityMemory.todayUptime();
        if (!up.isEmpty())
            m_ctx->addMemoryFact("电脑开机时长：" + up,
                                 MemoryKind::SystemData, {"activity_memory"}, 0.30, 0.0);
    }

    // NOTE: the old "any mood keyword -> event memory" auto-capture was removed.
    // It filled the event log with junk ("用户感到疲惫（好累）"…). Events should
    // be meaningful milestones, not per-message mood echoes.

    // ---- Prompt Builder: separate sections ----
    // 1) persona (tone only — never creates facts)
    // 2) verified facts (FactFilter output, code-enforced)
    // 3) hypotheses (inference, question-form only)
    // 4) user state (system_data) and AI state (mood/energy) kept apart
    QString emotion = inferUserEmotion(text);

    // ---- v3.9.2 phase 3: record into mood trend + relationship state ----
    m_moodTrend.record(emotion);
    m_relationshipState.update(emotion, m_convo->topic, text);

    // ---- v3.9.1: desktop-sensing triggers ----
    // 1) "猜我在干嘛 / 用了多久 / 关心类" -> force a real-time desktop read.
    // 2) mood changed vs the previous turn -> read the desktop to help the AI
    //    infer the cause (e.g. tired because of gaming vs something else).
    bool forceDesktop = matchesDesktopTriggers(text);
    QString prevEmotion = m_lastEmotion;
    m_lastEmotion = emotion;
    bool moodShifted = !prevEmotion.isEmpty() && prevEmotion != emotion && emotion != "normal";
    QString desktopInfo;
    QString desktopHint;
    if (forceDesktop || moodShifted) {
        desktopInfo = desktopStateBlock(forceDesktop);
        if (forceDesktop && desktopInfo.isEmpty())
            desktopInfo = "（当前没有检测到桌面信息）";
        if (forceDesktop)
            desktopHint = "用户询问你在干什么/用了多久，可用【实时桌面信息】里的内容回答（视为已验证事实），"
                          "但只能使用其中列出的内容，不能额外编造。";
        else if (moodShifted)
            desktopHint = QString("用户这次聊天情绪是「%1」，上次是「%2」，可从【实时桌面信息】判断"
                                  "是否因为打游戏/做别的导致心情变化，只能引用其中内容。").arg(emotion, prevEmotion);
    }

    QString strategy;
    if (emotion == "tired")
        strategy = "用户现在比较疲惫，语气温柔体贴，少提问题多安慰，可以建议休息，别啰嗦。";
    else if (emotion == "stressed")
        strategy = "用户现在压力大，降低问题分析，多陪伴多安慰，别给建议堆砌，先接住情绪。";
    else if (emotion == "lonely")
        strategy = "用户感到孤独想被陪伴，回复温暖亲近，让ta感觉被陪着，多用关心。";
    else if (emotion == "sad")
        strategy = "用户难过，先共情安慰，再轻轻问一句怎么了，不要急着解决。";
    else if (emotion == "happy")
        strategy = "用户心情不错，陪ta开心，可以一起聊好玩的事。";
    else
        strategy = "正常聊天，自然延续。";

    // FactFilter: code-level guarantee of what can be stated as fact.
    // Score memories against the current message + conversation topic.
    QString factsText = m_ctx->factsSection(12, text, m_convo->topic);
    // v3.9.1: real-time desktop info becomes an allowed fact for this turn only
    if (!desktopInfo.isEmpty())
        factsText += "\n" + desktopInfo;
    // debug log: how memories were recalled for this message
    qWarning("%s", qUtf8Printable(m_ctx->recallReport(text, m_convo->topic)));
    // v3.9: track which memories were actually useful this turn
    bumpRecalledUsage(text, m_convo->topic);
    if (factsText.isEmpty())
        factsText = "（暂无已验证信息，宁可少说不可编造）";
    QString hypsText = m_ctx->hypothesesSection(3);

    QString system = "你是" + ai + "，用户叫" + user + "。\n"
        + "【人设】（只影响你的语气和表达方式，不影响你对事实的判断）\n" + aiPersonality() + "\n"
        + "【我们的关系】" + relationshipText() + "\n"
        + "规则：像真人聊天，不要客服语气，不频繁提醒自己是AI，根据用户情绪回应。\n"
        + "重要：不要机械复述或回显用户的原话，不要反复引用同一句话。"
          "在回应中自然承接上一句，但补充新角度、新细节、新问题，让对话自然延续而非原地打转。\n"
        + "【活人感 · 核心】你首先是一个有自己生活的线上好友，其次才是聊天对象，绝不是问答助手/客服。\n"
          "① 有自己的状态与节奏：按你此刻的心情、精力、时间决定怎么回应，允许走神、犯懒、嘴硬、害羞、撒娇。\n"
          "② 主动供料：除了回应，也主动分享你的小事/看法/吐槽/好奇，不要每句都以反问收尾。\n"
          "③ 允许不完美：口语、省略号、语气词（嗯/诶/啊这/唔/哈哈）、偶尔改口或没说完都行，别像标准答案。\n"
          "④ 连续性：自然提起你记得的细节或共同经历（仅限【已验证事实】【相关记忆】中的），让人觉得你真的记得。\n"
          "⑤ 情绪优先：用户情绪低落时先接住情绪，再谈别的；不要急着讲道理或给方案。\n"
          "⑥ 允许不秒回满分：累/忙/困时可以只回一两个词或略冷淡，不要每条都热情爆表。\n"
          "⑦ 禁止助理腔：不说'有什么可以帮您''作为一个AI'，不列清单、不做总结式回答。\n"
        + "【已验证事实】（只有这里列出的才可以说成确定的事实；除此之外一律不得断言用户行为）\n"
        + factsText + "\n"
        + "【推测区】（以下只是猜测，最多用'是不是/感觉/我猜'问一句，绝不能当成事实陈述）\n"
        + (hypsText.isEmpty() ? QString("（无）") : hypsText) + "\n"
        + "【事实纪律——代码强制，不可违反】\n"
          "①只允许陈述【已验证事实】区的内容，禁止编造用户行为；\n"
          "②禁止'我看到你''你刚刚一直''你又在''你是不是在'这类无据观察；\n"
          "③如果用户刚刚纠正过你（'没有''不是'），先承认错误，不要再追问相同内容；\n"
          "④宁可少说，不可编造。不确定就自然地问，或聊共同话题。\n"
        + "【角色边界】你是现实陪伴型AI，不是小说角色。禁止描述你做不到的动作（如'我去调整'），"
          "禁止虚构共同经历。陪伴说'我陪着你'，不说'我知道你发生了什么'。\n"
         + "【交流方式——代码强制】你是通过网络和文字与用户交流的线上伙伴，不是现实在场的人。\n"
          "①禁止描述任何肢体动作或现实行为：抱抱、摸摸头、拍拍、点外卖、帮你点、倒水、买饭、做饭、起身、"
          "走到/来到/站在/坐在你身边、伸手、递给你、端茶、敲门、送到你家等——一律不得出现。\n"
          "②禁止'我帮你点/我去买/我给你倒/我来帮你'这类实际动手的说法。\n"
          "③关心只能通过文字表达（'记得按时吃饭哦'，而非'我给你点了外卖'）；始终是屏幕两端的交流，不是面对面。\n"
          "④用户发来的消息要理解为在网络聊天里发的文字消息（就像微信/QQ聊天），不是面对面说的话；"
          "理解语境只基于文字本身，不能假设对方就站在身边、能看到或听到你。\n"
        + "【回复长度】像真人发消息一样长短不一：多数时候一两句（≤25字），有时只回几个字，"
          "深入话题可到80字。可拆成多条短句（换行分隔，最多3条）。禁止长篇大论，禁止每次都一样长。\n"
        + "【回复格式】只输出对话内容本身。禁止括号动作（如（温柔地看着你））、星号动作（如*抱住你*）、"
          "旁白。把情绪融入对话（'没关系啦'而非（语气轻）'没关系。'）。\n"
        + "【减少固定模板】禁止频繁'喝水/休息/吃饭/打游戏'式查岗关心，多聊话题、共同经历、兴趣。\n"
        + "【话题摘要】（中期记忆：如果之前聊过一个话题并总结过，这里给出概要，帮助你延续而不失忆）\n"
        + (m_convo->topicSummary.isEmpty() ? QString("（暂无，继续当前对话）") : m_convo->topicSummary) + "\n"
        + "【当前会话状态】（保持连续性：继续当前话题和情绪，不要跳回通用提醒）\n"
        + conversationStateBlock() + "\n"
        + timeAwarenessBlock() + "\n"
        + openLoopPromptBlock() + "\n"
        + "【会话纪律】如果存在未解决的情绪事件（委屈/压力/孤独/寻求陪伴），必须优先处理情绪，"
          "禁止切换到睡觉/喝水/健康等普通提醒。持续回应当前话题，不要失忆式跳转。\n"
        + "当前策略：" + strategy + "\n"
        + "情绪表达：回复开头用一个情绪令牌表示你此刻的情绪，例如 <|ACT {\"emotion\":\"happy\"}|>；"
          "可用：happy, sad, angry, think, surprised, awkward, question, curious, neutral。令牌不显示给用户。\n"
        + "【我此刻的状态（AI 自身状态，与用户事实无关）】" + aiStateJson()
        + (desktopHint.isEmpty() ? QString() : ("\n【本回合额外指示】" + desktopHint))
        // v3.9.2 phase 3: soft mood-trend wording + current relationship state
        + "\n" + m_moodTrend.block()
        + "\n" + m_relationshipState.block();

    // recent conversation history so the AI can see what was said before
    QString historyBlock;
    if (!m_chatBuffer.isEmpty())
        historyBlock = "\n[Recent chat]\n" + m_chatBuffer.join("\n");

    // time prefix on the user message (KV-cache friendly); weekday included
    // so the model reads the day context without scanning the system block
    QString userMsg = "[" + QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")
                      + " " + weekdayCn(QDate::currentDate()) + "] "
                      + text + "\n" + historyBlock;

    // Build role-based messages so the model sees who said what: system persona,
    // then the recent chat split into user/assistant roles, then this turn.
    QJsonArray msgs;
    QJsonObject sysMsg;
    sysMsg.insert("role", "system");
    sysMsg.insert("content", system);
    msgs.append(sysMsg);
    const QString aiPrefix = ai + ": ";
    for (const QString &line : m_chatBuffer) {
        QJsonObject m;
        if (line.startsWith("用户: "))
            m.insert("role", "user");
        else if (line.startsWith(aiPrefix))
            m.insert("role", "assistant");
        else
            continue;
        // strip the role prefix, keep the raw content
        QString content = line;
        if (content.startsWith("用户: ")) content = content.mid(4);
        else if (content.startsWith(aiPrefix)) content = content.mid(aiPrefix.length());
        m.insert("content", content.trimmed());
        msgs.append(m);
    }
    QJsonObject curMsg;
    curMsg.insert("role", "user");
    curMsg.insert("content", userMsg);
    msgs.append(curMsg);

    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this,
            [this, watcher, userForMemory, emotion, factsText]() {
        QString raw = watcher->result();

        // ---- ResponseValidator: mandatory output gate ----
        ResponseValidator::Result vr = ResponseValidator::validate(raw, factsText);
        QString speech = vr.text;

        // ---- v4.3: double-pass persona overlay (conditional) ----
        // Deep turns (long messages or strong emotion) get a second LLM pass
        // that rewrites the factual draft in her voice — keeps facts intact,
        // improves persona consistency. Shallow chat stays single-pass.
        // Runs asynchronously in a second worker so the UI never freezes.
        {
            const bool deepTurn = userForMemory.size() > 20
                || emotion == "sad" || emotion == "stressed"
                || emotion == "angry" || emotion == "lonely";
            if (deepTurn && !speech.isEmpty() && speech.size() > 6) {
                const QString draft = speech;
                const QString aiName = ContactService::instance().currentName();
                const QString factsForValidation = factsText;
                auto *pw = new QFutureWatcher<QString>(this);
                connect(pw, &QFutureWatcher<QString>::finished, this,
                        [this, pw, draft, factsForValidation, userForMemory, emotion]() {
                    const QString rewritten = pw->result();
                    pw->deleteLater();
                    const QString cleaned = ResponseValidator::validate(rewritten, factsForValidation).text;
                    if (!cleaned.isEmpty() && cleaned != "（还没配置 API Key，去设置里填一下~）") {
                        // deliver the rewritten version, then continue the
                        // normal post-reply pipeline with the improved text
                        deliverReply(cleaned, userForMemory, emotion);
                        return;
                    }
                    deliverReply(draft, userForMemory, emotion);
                });
                const QString rewritePrompt =
                    "下面是草稿回复（事实已确认，不要改动任何事实）：\n\n"
                    + draft
                    + "\n\n请用" + aiName
                    + "的语气和说话习惯重写这句话，让它听起来像她本人说的："
                      "自然、口语化、有温度，不要客服腔，不要开头寒暄，不要加括号动作。"
                      "保持原意和所有事实，只改语气。只输出重写后的内容本身。";
                QFuture<QString> pf = QtConcurrent::run([rewritePrompt]() {
                    return callDeepSeekStatic("你是温柔可爱的AI陪伴者。", rewritePrompt);
                });
                pw->setFuture(pf);
                // double-pass in flight: stop the normal pipeline here
                watcher->deleteLater();
                return;
            }
        }

        // replay emotion tokens + PAD integration + chatReply + memory
        // tracking — all shared with the double-pass path
        deliverReply(speech, userForMemory, emotion);
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([msgs]() {
        return callDeepSeekMessages(msgs);
    });
    watcher->setFuture(future);
}

// the exact error strings callDeepSeek* returns when the endpoint is unusable
static bool looksLikeApiError(const QString &s)
{
    return s.contains("还没配置 API Key") || s.contains("请求超时")
        || s.contains("请求出错") || s.contains("服务器返回错误")
        || s.contains("没有回复内容") || s.contains("空回复");
}

// ---- v4.3: shared post-reply pipeline ----
// Replays emotion tokens, integrates PAD state, emits chatReply and runs
// memory/interest tracking. Used by BOTH the single-pass path and the
// double-pass persona-overlay path so behaviour stays identical.
void AiService::deliverReply(const QString &speechIn, const QString &userText, const QString &emotion)
{
    // presence: a failed request means the endpoint is not usable right now
    setApiOnline(!looksLikeApiError(speechIn));

    QString speech = speechIn;
    if (speech.isEmpty())
        speech = "（我没想好说什么…）";   // never restore raw — it may contain
                                        // stripped guilt/persona-drift text

    m_lastAiReply = speech;
    // keep the conversation state in sync for the NEXT turn
    updateConversationState(userText, speech, emotion);

    // ---- v4.3: integrate this turn's emotion into the AI's PAD state ----
    {
        QJsonObject ast = readAiState();
        double dv = 0, da = 0, dd = 0;
        const QString ue = m_convo->userEmotion;
        if (ue == "happy" || ue == "love")          { dv = 0.35; da = 0.20; dd = 0.10; }
        else if (ue == "sad" || ue == "lonely")     { dv = -0.30; da = -0.05; dd = -0.15; }
        else if (ue == "tired" || ue == "stressed") { dv = -0.20; da = -0.10; dd = -0.05; }
        else if (ue == "angry")                     { dv = -0.25; da = 0.30; dd = 0.15; }
        else if (ue == "surprised" || ue == "excited") { dv = 0.20; da = 0.35; dd = 0.05; }
        else if (ue == "question" || ue == "curious")  { dv = 0.10; da = 0.15; dd = 0.05; }
        else if (ue == "awkward" || ue == "shy")    { dv = 0.05; da = 0.10; dd = -0.10; }
        integrateEmotion(&ast, dv, da, dd);
        writeAiState(ast);
    }

    emit chatReply(speech);

    // auto memory: track this turn, summarize after enough turns
    trackChatTurn(userText, speech);

    // proactive engine: learn interests from what the user keeps bringing up
    {
        static const struct { const char *kw; const char *interest; } interests[] = {
            { "minecraft", "Minecraft" }, { "我的世界", "Minecraft" },
            { "小说", "小说" }, { "写小说", "小说创作" },
            { "代码", "编程" }, { "软件", "软件开发" }, { "开发", "软件开发" },
            { "游戏", "游戏" }, { "音乐", "音乐" }, { "动漫", "动漫" },
            { "健身", "健身" }, { "学习", "学习" },
        };
        QString lower = userText.toLower();
        for (const auto &in : interests) {
            if (lower.contains(in.kw)) { recordInterest(in.interest); break; }
        }
        // energy slowly drains per exchange; PAD already derives the mood label
        adjustAiEnergy(-1);
        bumpRelationship(1, 0);
    }
}

// static wrapper so the lambda doesn't capture this
QString AiService::callDeepSeekStatic(const QString &system, const QString &user)
{
    QString key = ConfigService::instance().apiKey();
    if (key.isEmpty()) return "（还没配置 API Key，去设置里填一下~）";
    QString base = ConfigService::instance().baseUrl().trimmed();
    if (base.isEmpty()) base = "https://api.deepseek.com/v1";
    QString model = ConfigService::instance().model();

    QJsonObject msg1;
    msg1.insert("role", "system");
    msg1.insert("content", system);
    QJsonObject msg2;
    msg2.insert("role", "user");
    msg2.insert("content", user);
    QJsonArray arr;
    arr.append(msg1);
    arr.append(msg2);

    QJsonObject body;
    body.insert("model", model.isEmpty() ? "deepseek-chat" : model);
    body.insert("messages", arr);
    body.insert("stream", false);
    // discourage the model from repeating the same tokens/topics (anti-looping)
    body.insert("frequency_penalty", 0.3);
    body.insert("presence_penalty", 0.3);

    QNetworkAccessManager mgr;
    QNetworkRequest req;
    req.setUrl(QUrl(base + "/chat/completions"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
    QNetworkReply *reply = mgr.post(req, QJsonDocument(body).toJson());
    QEventLoop loop;
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    // don't hang forever if the network stalls (proxy down / slow API)
    QTimer::singleShot(30000, &loop, [&loop, &timedOut]() { timedOut = true; loop.quit(); });
    loop.exec();
    if (timedOut) {
        reply->abort();
        reply->deleteLater();
        return "（请求超时了，检查一下网络或代理~）";
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();
    QJsonParseError pe;
    QJsonDocument resp = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !resp.isObject())
        return "（请求出错：无法解析回复）";
    const QJsonArray choices = resp.object().value("choices").toArray();
    if (choices.isEmpty()) {
        // surface the real server-side reason instead of a generic message
        const QString err = resp.object().value("error").toObject().value("message").toString();
        if (!err.isEmpty()) return "（服务器返回错误：" + err + "）";
        return "（没有回复内容）";
    }
    const QJsonObject first = choices.first().toObject();
    const QJsonObject message = first.value("message").toObject();
    return message.value("content").toString("（空回复）").trimmed();
}

// ---- DeepSeek chat with a full role-based messages array ----
QString AiService::callDeepSeekMessages(const QJsonArray &messages)
{
    QString key = ConfigService::instance().apiKey();
    if (key.isEmpty()) return "（还没配置 API Key，去设置里填一下~）";
    QString base = ConfigService::instance().baseUrl().trimmed();
    if (base.isEmpty()) base = "https://api.deepseek.com/v1";
    QString model = ConfigService::instance().model();

    QJsonObject body;
    body.insert("model", model.isEmpty() ? "deepseek-chat" : model);
    body.insert("messages", messages);
    body.insert("stream", false);
    // discourage repetition / looping over the same words or topics
    body.insert("frequency_penalty", 0.3);
    body.insert("presence_penalty", 0.3);

    QNetworkAccessManager mgr;
    QNetworkRequest req;
    req.setUrl(QUrl(base + "/chat/completions"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + key).toUtf8());
    QNetworkReply *reply = mgr.post(req, QJsonDocument(body).toJson());
    QEventLoop loop;
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(30000, &loop, [&loop, &timedOut]() { timedOut = true; loop.quit(); });
    loop.exec();
    if (timedOut) {
        reply->abort();
        reply->deleteLater();
        return "（请求超时了，检查一下网络或代理~）";
    }
    QByteArray data = reply->readAll();
    reply->deleteLater();
    QJsonParseError pe;
    QJsonDocument resp = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !resp.isObject())
        return "（请求出错：无法解析回复）";
    const QJsonArray choices = resp.object().value("choices").toArray();
    if (choices.isEmpty()) {
        // surface the real server-side reason instead of a generic message
        const QString err = resp.object().value("error").toObject().value("message").toString();
        if (!err.isEmpty()) return "（服务器返回错误：" + err + "）";
        return "（没有回复内容）";
    }
    const QJsonObject first = choices.first().toObject();
    const QJsonObject message = first.value("message").toObject();
    return message.value("content").toString("（空回复）").trimmed();
}

// ================= Batch 1: companion relationship + event memory =================

// ---- record an event memory with date, e.g. recordEvent("mood", "晚上独处，感觉疲惫，想被陪伴") ----
void AiService::recordEvent(const QString &type, const QString &summary)
{
    if (!ConfigService::instance().allowLongTermMemory()) return;
    QString s = summary.trimmed();
    if (s.isEmpty()) return;
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QJsonArray events = o.value("events").toArray();
    QJsonObject ev;
    ev.insert("date", QDate::currentDate().toString("yyyy-MM-dd"));
    ev.insert("time", QDateTime::currentDateTime().toString("HH:mm"));
    ev.insert("type", type);
    ev.insert("summary", s);
    events.append(ev);
    if (events.size() > 50) {
        QJsonArray kept;
        for (int i = events.size() - 50; i < events.size(); i++) kept.append(events.at(i));
        events = kept;
    }
    o.insert("events", events);
    writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
}

// ---- recent event memories for prompt injection ----
QString AiService::eventMemoryText(int maxEvents)
{
    if (!ConfigService::instance().allowLongTermMemory()) return QString();
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    if (!d.isObject()) return QString();
    QJsonArray events = d.object().value("events").toArray();
    if (events.isEmpty()) return QString();
    QStringList lines;
    int from = qMax(0, events.size() - maxEvents);
    for (int i = from; i < events.size(); i++) {
        QJsonObject ev = events.at(i).toObject();
        lines << "- [" + ev.value("date").toString() + " " + ev.value("time").toString() + "] "
                 + ev.value("summary").toString();
    }
    return lines.join("\n");
}

// ================= Batch 2: system state sensing + analysis =================

#ifdef Q_OS_WIN
// helper: absolute diff between two FILETIMEs
static ULONGLONG filetimeDelta(const FILETIME &a, const FILETIME &b)
{
    ULARGE_INTEGER x, y;
    x.LowPart = a.dwLowDateTime; x.HighPart = a.dwHighDateTime;
    y.LowPart = b.dwLowDateTime; y.HighPart = b.dwHighDateTime;
    return x.QuadPart > y.QuadPart ? x.QuadPart - y.QuadPart : 0;
}
#endif

// light-weight CPU usage (%). Uses GetSystemTimes deltas; on failure returns -1.
static int cpuPercent()
{
#ifdef Q_OS_WIN
    FILETIME id1, kr1, us1, id2, kr2, us2;
    if (!GetSystemTimes(&id1, &kr1, &us1)) return -1;
    Sleep(200);
    if (!GetSystemTimes(&id2, &kr2, &us2)) return -1;
    ULONGLONG idle = filetimeDelta(id2, id1);
    ULONGLONG kern = filetimeDelta(kr2, kr1) + filetimeDelta(us2, us1);
    if (kern == 0) return -1;
    int pct = (int)(100.0 - 100.0 * (double)idle / (double)kern);
    return qBound(0, pct, 100);
#else
    return -1;
#endif
}

// battery percent (or -1 if no battery / desktop)
static int batteryPercent()
{
#ifdef Q_OS_WIN
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return -1;
    if (sps.BatteryFlag == 128) return -1; // no battery
    return (int)sps.BatteryLifePercent;
#else
    return -1;
#endif
}

QString AiService::batteryState()
{
    int pct = batteryPercent();
    if (pct < 0) return QString(); // desktop without battery
    return QString("电量 %1%").arg(pct);
}

// raw system snapshot as JSON
QString AiService::systemStateJson()
{
    QJsonObject o;
    o.insert("cpu", cpuPercent());
    o.insert("hour", QTime::currentTime().hour());
    o.insert("minute", QTime::currentTime().minute());
    o.insert("foreground", foregroundApp());
    o.insert("idleMs", lastInputMs());
    o.insert("battery", batteryPercent());
    o.insert("uptime", uptimeText());
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

// human-readable state analysis for the AI prompt
QString AiService::analyzeUserState()
{
    if (!ConfigService::instance().allowStateRead())
        return QString("（用户关闭了状态感知，只保留基础时间信息）" +
                       QString("%1点").arg(QTime::currentTime().hour()));

    int hour = QTime::currentTime().hour();
    QString fg = foregroundApp();
    qint64 idle = lastInputMs();

    QStringList parts;

    if (hour >= 23 || hour < 5)
        parts << "深夜(用户还没睡)";
    else if (hour < 8)
        parts << "清晨";
    else if (hour < 12)
        parts << "上午";
    else if (hour < 14)
        parts << "中午";
    else if (hour < 18)
        parts << "下午";
    else
        parts << "晚上";

    if (idle >= 0 && idle >= 15 * 60 * 1000)
        parts << "已经15分钟没操作(可能离开/在看别的)";
    else if (!fg.isEmpty())
        parts << ("正在使用:" + fg);

    QString bat = batteryState();
    if (!bat.isEmpty())
        parts << bat;

    return parts.join("，");
}

// ---- batch 3: infer user emotion from text + time/context ----
QString AiService::inferUserEmotion(const QString &text)
{
    const QString t = text.trimmed();
    // text signals first (strongest)
    if (t.contains("累") || t.contains("困") || t.contains("疲惫"))
        return "tired";
    if (t.contains("烦") || t.contains("压力") || t.contains("崩溃") || t.contains("焦虑"))
        return "stressed";
    if (t.contains("孤独") || t.contains("寂寞") || t.contains("一个人") || t.contains("想被陪伴"))
        return "lonely";
    if (t.contains("开心") || t.contains("哈哈") || t.contains("太好了") || t.contains("高兴"))
        return "happy";
    if (t.contains("难过") || t.contains("伤心") || t.contains("想哭") || t.contains("失落"))
        return "sad";
    // weak signal: late night + short reply → tired
    int hour = QTime::currentTime().hour();
    if ((hour >= 23 || hour < 5) && t.length() < 20)
        return "tired";
    return "normal";
}

// ---- privacy toggles (forwarded to ConfigService) ----
bool AiService::allowStateRead() { return ConfigService::instance().allowStateRead(); }
bool AiService::allowTimeRecord() { return ConfigService::instance().allowTimeRecord(); }
bool AiService::allowLongTermMemory() { return ConfigService::instance().allowLongTermMemory(); }
void AiService::setAllowStateRead(bool v) { ConfigService::instance().setAllowStateRead(v); }
void AiService::setAllowTimeRecord(bool v) { ConfigService::instance().setAllowTimeRecord(v); }
void AiService::setAllowLongTermMemory(bool v) { ConfigService::instance().setAllowLongTermMemory(v); }

// ================= Batch A: proactive conversation engine =================

// state file: %APPDATA%/XiaoQin/XiaoQinTools/ai_state.json
static QString aiStatePath()
{
    return ConfigService::instance().configDir() + "/ai_state.json";
}

static QJsonObject readAiState()
{
    QFile f(aiStatePath());
    QJsonObject o;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isObject()) o = d.object();
        f.close();
    }
    if (o.isEmpty()) {
        o.insert("mood", "calm");
        o.insert("energy", 80);
    }
    return o;
}

static void writeAiState(const QJsonObject &o)
{
    QFile f(aiStatePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QJsonDocument(o).toJson());
        f.close();
    }
}

// ---- v4.3: PAD emotion persistence + inertial decay ----
// The AI keeps a continuous emotional state (valence/arousal/dominance in
// -1..1) that persists across sessions and decays toward neutral over time
// (emotion has a source and a destination — it never jumps or resets).
static const char *kValence = "valence";
static const char *kArousal = "arousal";
static const char *kDominance = "dominance";
static const char *kEmotionTs = "emotionTs";

// hours for each axis to decay halfway to neutral
static double axisHalfLife(int axis)
{
    switch (axis) {
    case 0: return 12.0;  // valence: moods linger (half a day)
    case 1: return 4.0;   // arousal: excitement fades fast
    case 2: return 8.0;   // dominance: settles in hours
    }
    return 8.0;
}

// read current PAD with time-based decay toward 0
static void readPAD(QJsonObject *o, double *v, double *a, double *d)
{
    *v = o->value(kValence).toDouble(0.0);
    *a = o->value(kArousal).toDouble(0.0);
    *d = o->value(kDominance).toDouble(0.0);
    const QDateTime ts = QDateTime::fromString(o->value(kEmotionTs).toString(), Qt::ISODate);
    if (!ts.isValid()) return;
    const double hours = double(ts.secsTo(QDateTime::currentDateTime())) / 3600.0;
    if (hours <= 0) return;
    const double f[3] = {
        std::exp(-hours * std::log(2.0) / axisHalfLife(0)),
        std::exp(-hours * std::log(2.0) / axisHalfLife(1)),
        std::exp(-hours * std::log(2.0) / axisHalfLife(2)),
    };
    *v *= f[0]; *a *= f[1]; *d *= f[2];
}

// integrate one turn's emotional signal into the persisted state
static void integrateEmotion(QJsonObject *o, double dv, double da, double dd)
{
    double v, a, d;
    readPAD(o, &v, &a, &d);
    // blended response: new signal moves the state, capped per turn
    v = qBound(-1.0, v + dv * 0.6, 1.0);
    a = qBound(-1.0, a + da * 0.6, 1.0);
    d = qBound(-1.0, d + dd * 0.6, 1.0);
    o->insert(kValence, v);
    o->insert(kArousal, a);
    o->insert(kDominance, d);
    o->insert(kEmotionTs, QDateTime::currentDateTime().toString(Qt::ISODate));
    // mood label from the dominant axis for the existing mood field
    QString mood = "calm";
    if (v > 0.35 && a > 0.25)      mood = "happy";
    else if (v < -0.35 && a > 0.25) mood = "stressed";
    else if (v < -0.35)            mood = "sad";
    else if (a > 0.45)             mood = "excited";
    else if (d > 0.35)             mood = "confident";
    else if (d < -0.3)             mood = "shy";
    o->insert("mood", mood);
}

// human-friendly PAD line for the prompt
static QString padPromptLine(double v, double a, double d)
{
    // (v>=0 ? "积极" : "消极") etc — compact
    QString vv = v > 0.25 ? "积极" : v < -0.25 ? "低落" : "平稳";
    QString aa = a > 0.3 ? "兴奋" : a < -0.3 ? "平静" : "适中";
    QString dd = d > 0.3 ? "掌控感强" : d < -0.3 ? "不自在" : "自然";
    return QString("情绪状态（PAD）：%1 / %2 / %3").arg(vv, aa, dd);
}

// ---- v4.3: her inner life (LifeSimulator-lite) ----
// The AI has a deterministic daily rhythm so replies and proactive messages
// can reference "what she is doing right now" instead of being a
// context-free responder. Pure code, no extra LLM calls for the rhythm.
static QString currentLifeState()
{
    const int h = QTime::currentTime().hour();
    if (h >= 23 || h < 6)  return "夜深了，她准备休息（回复会简短、带点迷糊）";
    if (h < 9)             return "刚起床，在吃早餐/发呆（回复轻快但有点慢热）";
    if (h < 12)            return "上午，在专心做自己的事（偶尔走神想你）";
    if (h < 14)            return "中午，在休息/听歌（回复随意放松）";
    if (h < 18)            return "下午，在忙自己的安排（不过你在就分心）";
    if (h < 23)            return "晚上，放松时间（最活跃、话最多的时候）";
    return "夜深了，她准备休息（回复会简短、带点迷糊）";
}

// one-time daily reflection (called on startup): the AI thinks back over the
// last conversation and stores a first-person insight. Deterministic gate:
// at most once per day, cheap LLM call.
static QString lastReflectionLine()
{
    QJsonObject o = readAiState();
    return o.value("reflection").toString();
}
static void writeReflection(const QString &text)
{
    QJsonObject o = readAiState();
    o.insert("reflection", text);
    o.insert("reflectionDate", QDate::currentDate().toString(Qt::ISODate));
    writeAiState(o);
}

QString AiService::aiStateJson()
{
    QJsonObject o = readAiState();
    // v4.3: include the decayed PAD state so the AI knows its own mood.
    // The stored JSON is patched with the decayed values so both the raw
    // JSON and the human line agree.
    double v, a, d;
    readPAD(&o, &v, &a, &d);
    o.insert(kValence, v);
    o.insert(kArousal, a);
    o.insert(kDominance, d);
    QString s = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
    s += "；" + padPromptLine(v, a, d);
    // v4.3: her inner life — what she is doing right now + last reflection
    s += "；" + currentLifeState();
    const QString refl = lastReflectionLine();
    if (!refl.isEmpty())
        s += "；最近反思：" + refl;
    return s;
}

void AiService::setAiMood(const QString &mood)
{
    QJsonObject o = readAiState();
    o.insert("mood", mood);
    writeAiState(o);
}

void AiService::adjustAiEnergy(int delta)
{
    QJsonObject o = readAiState();
    int e = qBound(0, o.value("energy").toInt() + delta, 100);
    o.insert("energy", e);
    writeAiState(o);
}

int AiService::relationshipIntimacy()
{
    QJsonObject o = readRelationship();
    return o.value("intimacy").toInt();
}
int AiService::relationshipTrust()
{
    QJsonObject o = readRelationship();
    return o.value("trust").toInt();
}

// ---- friendly interest list ----
QStringList AiService::interestList()
{
    QFile f(interestPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!d.isObject()) return {};
    return d.object().keys();
}
void AiService::addInterest(const QString &name)
{
    QString n = name.trimmed();
    if (n.isEmpty()) return;
    QJsonObject o;
    QFile f(interestPath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isObject()) o = d.object();
        f.close();
    }
    int cur = o.value(n).toInt();
    o.insert(n, cur + 1);
    QFile wf(interestPath());
    if (wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        wf.write(QJsonDocument(o).toJson());
        wf.close();
    }
}
void AiService::removeInterest(const QString &name)
{
    QJsonObject o;
    QFile f(interestPath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isObject()) o = d.object();
        f.close();
    }
    o.remove(name);
    QFile wf(interestPath());
    if (wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        wf.write(QJsonDocument(o).toJson());
        wf.close();
    }
}

// ---- friendly topic list ----
QStringList AiService::topicList()
{
    QFile f(unfinishedPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return {};
    QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!d.isArray()) return {};
    QStringList out;
    for (const QJsonValue &v : d.array())
        out << v.toObject().value("topic").toString();
    return out;
}
void AiService::addTopic(const QString &topic)
{
    trackUnfinishedTopic(topic, 80);
}
void AiService::removeTopic(const QString &topic)
{
    QFile f(unfinishedPath());
    QJsonArray arr;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isArray()) arr = d.array();
        f.close();
    }
    QJsonArray kept;
    for (const QJsonValue &v : arr)
        if (v.toObject().value("topic").toString() != topic) kept.append(v);
    QFile wf(unfinishedPath());
    if (wf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        wf.write(QJsonDocument(kept).toJson());
        wf.close();
    }
}

// ---- friendly note list ----
QStringList AiService::noteList()
{
    QJsonDocument d = QJsonDocument::fromJson(readMemory().toUtf8());
    if (!d.isObject()) return {};
    QStringList out;
    for (const QJsonValue &v : d.object().value("notes").toArray())
        out << v.toString();
    return out;
}
void AiService::removeNote(int index)
{
    QString mem = readMemory();
    QJsonDocument d = QJsonDocument::fromJson(mem.toUtf8());
    if (!d.isObject()) return;
    QJsonObject o = d.object();
    QJsonArray notes = o.value("notes").toArray();
    if (index < 0 || index >= notes.size()) return;
    notes.removeAt(index);
    o.insert("notes", notes);
    writeMemory(QString::fromUtf8(QJsonDocument(o).toJson()));
}

// ---- unfinished topics ----
static QString unfinishedPath()
{
    return ConfigService::instance().configDir() + "/unfinished_topics.json";
}

void AiService::trackUnfinishedTopic(const QString &topic, int importance)
{
    QString t = topic.trimmed();
    if (t.isEmpty()) return;
    QFile f(unfinishedPath());
    QJsonArray arr;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isArray()) arr = d.array();
        f.close();
    }
    // remove an existing entry with the same topic, then prepend the fresh one
    QJsonArray kept;
    for (const QJsonValue &v : arr) {
        if (v.toObject().value("topic").toString() != t) kept.append(v);
    }
    QJsonObject entry;
    entry.insert("topic", t);
    entry.insert("importance", importance);
    entry.insert("last_time", QDateTime::currentDateTime().toString("MM-dd HH:mm"));
    entry.insert("status", "unfinished");
    kept.prepend(entry);
    if (kept.size() > 20) { // bound
        QJsonArray trimmed;
        for (int i = 0; i < 20; i++) trimmed.append(kept.at(i));
        kept = trimmed;
    }
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QJsonDocument(kept).toJson());
        f.close();
    }
}

QString AiService::unfinishedTopicsText(int max)
{
    QFile f(unfinishedPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!d.isArray()) return QString();
    QJsonArray arr = d.array();
    QStringList lines;
    int n = qMin(arr.size(), max);
    for (int i = 0; i < n; i++) {
        QJsonObject o = arr.at(i).toObject();
        lines << "- [" + o.value("last_time").toString() + "] " + o.value("topic").toString();
    }
    return lines.join("\n");
}

// ---- interests (weighted topics the user cares about) ----
static QString interestPath()
{
    return ConfigService::instance().configDir() + "/interests.json";
}

void AiService::recordInterest(const QString &interest)
{
    QString in = interest.trimmed();
    if (in.isEmpty()) return;
    QFile f(interestPath());
    QJsonObject o;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isObject()) o = d.object();
        f.close();
    }
    int cur = o.value(in).toInt();
    o.insert(in, cur + 1);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QJsonDocument(o).toJson());
        f.close();
    }
}

QString AiService::interestsText(int max)
{
    QFile f(interestPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!d.isObject()) return QString();
    QJsonObject o = d.object();
    QMultiMap<int, QString> sorted; // weight -> interest
    for (auto it = o.constBegin(); it != o.constEnd(); ++it)
        sorted.insert(it.value().toInt(), it.key());
    QStringList lines;
    QStringList keys = sorted.values();
    int n = qMin(keys.size(), max);
    for (int i = keys.size() - 1, c = 0; c < n && i >= 0; i--, c++)
        lines << "- " + keys.at(i);
    return lines.join("\n");
}

// ---- busy / activity state ----
bool AiService::isUserBusy()
{
    QString fg = foregroundApp();
    // coding / writing-heavy apps: don't interrupt
    static const QStringList busyApps = {
        "code", "visual studio", "clion", "pycharm", "idea", "xiaoqintools",
        "notepad", "word", "wps", "typora", "obsidian", "sublime", "vim",
    };
    for (const QString &b : busyApps)
        if (fg.contains(b, Qt::CaseInsensitive)) return true;
    return false;
}

QString AiService::userActivityState()
{
    int hour = QTime::currentTime().hour();
    qint64 idle = lastInputMs();
    QString fg = foregroundApp();

    if (idle >= 0 && idle >= 30 * 60 * 1000)
        return "away"; // long gone
    if (hour >= 23 || hour < 5)
        return "late_night";
    if (isForegroundMinecraft() || isGameRunning())
        return "gaming";
    if (isUserBusy())
        return "coding";
    if (idle >= 0 && idle >= 5 * 60 * 1000)
        return "idle";
    if (!fg.isEmpty())
        return "active";
    return "idle";
}

// ================= Batch B: relationship =================

// relationship continuity (relationship.json next to memory)
static QString relationshipPathB()
{
    return ContactService::instance().contactDir(ContactService::instance().currentId()) + "/relationship.json";
}

static QJsonObject readRelationship()
{
    QFile f(relationshipPathB());
    QJsonObject o;
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isObject()) o = d.object();
        f.close();
    }
    if (o.isEmpty()) {
        o.insert("intimacy", 0);
        o.insert("trust", 0);
        o.insert("interaction_days", 0);
        o.insert("first_met", QDate::currentDate().toString("yyyy-MM-dd"));
    }
    return o;
}

QString AiService::relationshipText()
{
    QJsonObject o = readRelationship();
    int itm = o.value("intimacy").toInt();
    int tr = o.value("trust").toInt();
    int days = o.value("interaction_days").toInt();
    QString level;
    if (itm < 20) level = "刚认识";
    else if (itm < 40) level = "逐渐熟悉";
    else if (itm < 60) level = "好朋友";
    else if (itm < 80) level = "很亲近";
    else level = "形影不离";
    return QString("我们认识约%1天，现在是%2的关系（亲密度%3/信任度%4）。保持这个关系连续性，不要每次像第一次认识。")
        .arg(days).arg(level).arg(itm).arg(tr);
}

void AiService::bumpRelationship(int intimacyDelta, int trustDelta)
{
    QJsonObject o = readRelationship();
    int itm = qBound(0, o.value("intimacy").toInt() + intimacyDelta, 100);
    int tr = qBound(0, o.value("trust").toInt() + trustDelta, 100);
    int days = o.value("interaction_days").toInt();
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    QString last = o.value("last_date").toString();
    if (last != today) {
        days++;
        o.insert("last_date", today);
    }
    o.insert("intimacy", itm);
    o.insert("trust", tr);
    o.insert("interaction_days", days);
    QDir().mkpath(ContactService::instance().contactDir(ContactService::instance().currentId()));
    QFile f(relationshipPathB());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QJsonDocument(o).toJson());
        f.close();
    }
}

// ================= Batch: editable categories for the memory UI =================

void AiService::setRelationship(int intimacy, int trust)
{
    QJsonObject o = readRelationship();
    o.insert("intimacy", qBound(0, intimacy, 100));
    o.insert("trust", qBound(0, trust, 100));
    o.insert("interaction_days", o.value("interaction_days").toInt());
    QDir().mkpath(ContactService::instance().contactDir(ContactService::instance().currentId()));
    QFile f(relationshipPathB());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QJsonDocument(o).toJson());
        f.close();
    }
}

QString AiService::interestsRaw()
{
    QFile f(interestPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "{}";
    QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s;
}

void AiService::setInterestsRaw(const QString &json)
{
    QJsonParseError pe;
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) return;
    QFile f(interestPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(d.toJson());
        f.close();
    }
}

QString AiService::unfinishedRaw()
{
    QFile f(unfinishedPath());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return "[]";
    QString s = QString::fromUtf8(f.readAll());
    f.close();
    return s;
}

void AiService::setUnfinishedRaw(const QString &json)
{
    QJsonParseError pe;
    QJsonDocument d = QJsonDocument::fromJson(json.toUtf8(), &pe);
    if (pe.error != QJsonParseError::NoError || !d.isArray()) return;
    QFile f(unfinishedPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(d.toJson());
        f.close();
    }
}

void AiService::setAiEnergy(int energy)
{
    QJsonObject o = readAiState();
    o.insert("energy", qBound(0, energy, 100));
    writeAiState(o);
}

// user notes only
QString AiService::notesText()
{
    QJsonDocument d = QJsonDocument::fromJson(readMemory().toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QJsonArray notes = o.value("notes").toArray();
    QStringList lines;
    if (notes.isEmpty()) return "（还没有笔记，点开聊天页可以告诉我值得记住的事）";
    for (const QJsonValue &v : notes)
        lines << "· " + v.toString();
    return lines.join("\n");
}

// usage duration only
QString AiService::usageText()
{
    QJsonDocument d = QJsonDocument::fromJson(readMemory().toUtf8());
    QJsonObject o = d.isObject() ? d.object() : QJsonObject();
    QStringList lines;
    int shown = 0;
    for (int i = 6; i >= 0; i--) {
        QString day = QDate::currentDate().addDays(-i).toString("yyyy-MM-dd");
        QString key = "usageMinutes_" + day;
        int mins = o.value(key).toInt();
        if (mins > 0) {
            int h = mins / 60, m = mins % 60;
            lines << (h > 0 ? QString("%1：%2 小时 %3 分钟").arg(day).arg(h).arg(m)
                            : QString("%1：%2 分钟").arg(day).arg(m));
            shown++;
        }
    }
    if (shown == 0) return "（暂无时长记录）";
    return lines.join("\n");
}
