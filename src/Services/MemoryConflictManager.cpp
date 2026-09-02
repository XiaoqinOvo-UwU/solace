#include "MemoryConflictManager.h"
#include "MemoryImportanceEvaluator.h"
#include "MemoryRetriever.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include <algorithm>

QString MemoryConflictManager::m_path;
QJsonObject MemoryConflictManager::m_ledger;
QSet<QString> MemoryConflictManager::m_deprecatedCache;
bool MemoryConflictManager::m_dirty = false;

void MemoryConflictManager::setLedgerPath(const QString &path)
{
    m_path = path;
    reload();
}

QString MemoryConflictManager::ledgerPath() { return m_path; }

// strip trailing "（yyyy-MM-dd HH:mm）" suffix and any surrounding spaces
QString MemoryConflictManager::normalize(const QString &content)
{
    static const QRegularExpression suffixRe("[（(][0-9]{4}-[0-9]{2}-[0-9]{2}[ ][0-9:]+[)）]\\s*$");
    QString n = content.simplified();
    n.remove(suffixRe);
    n = n.trimmed();
    return n;
}

bool MemoryConflictManager::containsUpdateSignal(const QString &userText)
{
    static const QStringList updateSignals = {
        "不玩", "不喝", "不打了", "不看了", "不读了", "不追", "不做了",
        "不写了", "不用了", "不再", "退坑", "卸载", "删了", "戒了",
        "放弃了", "改玩", "换游戏", "开始玩", "现在只玩", "早就不",
        "已经不喜欢", "已经没玩", "没再玩", "最近不玩",
    };
    for (const QString &s : updateSignals)
        if (userText.contains(s)) return true;
    return false;
}

QString MemoryConflictManager::reasonForUpdate(const QString &userText)
{
    static const QStringList strong = { "退坑", "卸载", "删了", "戒了", "放弃了" };
    for (const QString &s : strong)
        if (userText.contains(s)) return "用户主动退出/放弃";
    static const QStringList replace = { "改玩", "换游戏", "现在只玩", "开始玩" };
    for (const QString &s : replace)
        if (userText.contains(s)) return "用户改变了偏好";
    return "用户主动更新";
}

// extract the topic subject from an update statement, e.g.
//   "我现在不玩Apex了" -> "Apex"
//   "我已经不喜欢喝可乐了" -> "可乐"
//   "我戒了游戏" -> "游戏"
static QString extractSubject(const QString &userText)
{
    QString t = userText.trimmed();
    // cut at the first update signal
    static const QStringList verbs = { "不玩", "不喝", "不打了", "不看了", "不读了",
                                       "不追", "不做了", "不写了", "不用了", "不再",
                                       "已经不喜欢", "不喜欢", "早就不", "没再玩" };
    int cut = -1, verbLen = 0;
    for (const QString &v : verbs) {
        int i = t.indexOf(v);
        if (i >= 0 && (cut < 0 || i < cut)) { cut = i; verbLen = v.size(); }
    }
    // also try single-verb "退坑/卸载/删了/戒了/放弃" (subject usually follows)
    static const QStringList drop = { "退坑", "卸载", "删了", "戒了", "放弃了", "放弃" };
    for (const QString &v : drop) {
        int i = t.indexOf(v);
        if (i >= 0 && (cut < 0 || i < cut)) { cut = i; verbLen = v.size(); }
    }
    if (cut < 0) return QString();

    QString sub = t.mid(cut + verbLen);
    // strip trailing particles / punctuation
    static const QRegularExpression tailRe("[了啊吧啦呢呀。！？,.!?]+\\s*$");
    sub.remove(tailRe);
    // strip leading temporal/emphatic words
    static const QRegularExpression headRe("^(我|现在|已经|以后|反正|真的|真的不|早就|最近|就|改|换)\\s*");
    sub.remove(headRe);
    sub = sub.trimmed();
    if (sub.isEmpty()) return QString();
    // a meaningful subject is at least 2 chars (avoid "它/这/那")
    return sub.size() >= 2 ? sub : QString();
}

MemoryConflictManager::Resolution MemoryConflictManager::resolve(
    const QString &userText, const QStringList &existingNotes)
{
    Resolution r;
    if (!containsUpdateSignal(userText)) return r;

    const QString subject = extractSubject(userText);
    if (subject.isEmpty()) return r;

    // 1) find old notes about this subject and deprecate them (never delete)
    for (const QString &note : existingNotes) {
        const QString n = normalize(note);
        if (n.isEmpty()) continue;
        if (n.contains(subject) && (n.contains("喜欢") || n.contains("爱") ||
                                    n.contains("玩") || n.contains("习惯") ||
                                    n.contains("常") || n.contains("追") ||
                                    n.contains("喝") || n.contains("用"))) {
            if (isDeprecated(note)) continue;      // already handled
            recordDeprecation(note, reasonForUpdate(userText), QString());
            r.deprecated << note;
            r.hadConflict = true;
        }
    }

    // 2) build the replacement active fact, e.g. "用户不玩Apex了"
    if (r.hadConflict || containsUpdateSignal(userText)) {
        QString cleaned = userText.simplified();
        static const QRegularExpression leadRe("^(我|我现在|我最近|其实|我觉得|我感觉|嗯|呃|那个)+\\s*");
        cleaned.remove(leadRe);
        cleaned = cleaned.trimmed();
        if (!cleaned.startsWith("用户"))
            cleaned = "用户" + cleaned;
        cleaned = normalize(cleaned);
        if (!cleaned.isEmpty() && cleaned.size() <= 40) {
            bool dup = false;
            for (const QString &note : existingNotes)
                if (normalize(note) == cleaned) { dup = true; break; }
            if (!dup && !r.created.contains(cleaned)) {
                r.created << cleaned;
                // record the replacement relation
                for (const QString &oldNote : r.deprecated)
                    recordDeprecation(oldNote, reasonForUpdate(userText), cleaned);
            }
        }
    }
    return r;
}

void MemoryConflictManager::recordDeprecation(const QString &oldNote,
                                              const QString &reason,
                                              const QString &replacedBy)
{
    QJsonArray conflicts = m_ledger.value("conflicts").toArray();
    const QString key = normalize(oldNote);
    // update in place if the same old note already has an entry
    for (int i = 0; i < conflicts.size(); ++i) {
        QJsonObject e = conflicts.at(i).toObject();
        if (e.value("old").toString() == key) {
            e.insert("status", "deprecated");
            e.insert("reason", reason);
            if (!replacedBy.isEmpty()) e.insert("replacedBy", replacedBy);
            e.insert("time", QDateTime::currentDateTime().toString(Qt::ISODate));
            conflicts[i] = e;
            m_ledger.insert("conflicts", conflicts);
            m_dirty = true;
            return;
        }
    }
    QJsonObject e;
    e.insert("old", key);
    e.insert("status", "deprecated");
    e.insert("reason", reason);
    if (!replacedBy.isEmpty()) e.insert("replacedBy", replacedBy);
    e.insert("time", QDateTime::currentDateTime().toString(Qt::ISODate));
    conflicts.append(e);
    if (conflicts.size() > 200) { // bound
        QJsonArray kept;
        for (int i = conflicts.size() - 200; i < conflicts.size(); ++i)
            kept.append(conflicts.at(i));
        conflicts = kept;
    }
    m_ledger.insert("conflicts", conflicts);
    m_deprecatedCache.insert(key);
    m_dirty = true;
}

bool MemoryConflictManager::isDeprecated(const QString &content)
{
    const QString key = normalize(content);
    if (m_deprecatedCache.contains(key)) return true;
    // fall back to scanning the ledger if the cache wasn't built yet
    const QJsonArray conflicts = m_ledger.value("conflicts").toArray();
    for (const QJsonValue &v : conflicts) {
        QJsonObject e = v.toObject();
        if (e.value("old").toString() == key &&
            e.value("status").toString() != "active") {
            m_deprecatedCache.insert(key);
            return true;
        }
    }
    return false;
}

QSet<QString> MemoryConflictManager::deprecatedSet()
{
    // warm the cache
    const QJsonArray conflicts = m_ledger.value("conflicts").toArray();
    for (const QJsonValue &v : conflicts) {
        QJsonObject e = v.toObject();
        if (e.value("status").toString() != "active")
            m_deprecatedCache.insert(e.value("old").toString());
    }
    return m_deprecatedCache;
}

QString MemoryConflictManager::replacementFor(const QString &content)
{
    const QString key = normalize(content);
    const QJsonArray conflicts = m_ledger.value("conflicts").toArray();
    for (const QJsonValue &v : conflicts) {
        QJsonObject e = v.toObject();
        if (e.value("old").toString() == key)
            return e.value("replacedBy").toString();
    }
    return QString();
}

void MemoryConflictManager::bumpUsage(const QString &content)
{
    const QString key = normalize(content);
    if (key.isEmpty()) return;
    QJsonObject usage = m_ledger.value("usage").toObject();
    usage.insert(key, usage.value(key).toInt() + 1);
    m_ledger.insert("usage", usage);
    QJsonObject last = m_ledger.value("lastUsed").toObject();
    last.insert(key, QDateTime::currentDateTime().toString(Qt::ISODate));
    m_ledger.insert("lastUsed", last);
    strengthen(key); // recall reinforces the memory
    m_dirty = true;
}

// ---- memory lifecycle: reinforcement + forgetting curve ----
void MemoryConflictManager::strengthen(const QString &content)
{
    const QString key = normalize(content);
    if (key.isEmpty()) return;
    QJsonObject s = m_ledger.value("strength").toObject();
    const double cur = s.value(key).toDouble(1.0);
    s.insert(key, qMin(2.0, cur + 0.10));
    m_ledger.insert("strength", s);
    m_dirty = true;
}

double MemoryConflictManager::strength(const QString &content)
{
    return m_ledger.value("strength").toObject().value(normalize(content)).toDouble(1.0);
}

double MemoryConflictManager::halfLifeDays(MemoryKind kind)
{
    switch (kind) {
    case MemoryKind::UserFact:     return 90.0;  // user said it directly
    case MemoryKind::HabitMemory:  return 60.0;  // repeated behavior
    case MemoryKind::MemoryEvent:  return 30.0;  // shared experience
    case MemoryKind::SystemData:   return 30.0;
    case MemoryKind::MemorySummary:return 14.0;  // AI interpretation — fades fast
    }
    return 30.0;
}

// strength x forgetting curve: exp(-ageDays*ln2 / halfLife)
// (halfLife is a true half-life: after `hl` days the remaining boost halves)
double MemoryConflictManager::effectiveStrength(const QString &content)
{
    const QString key = normalize(content);
    if (key.isEmpty()) return 1.0;
    const double raw = m_ledger.value("strength").toObject().value(key).toDouble(1.0);
    const QString lastUsed = m_ledger.value("lastUsed").toObject().value(key).toString();
    if (lastUsed.isEmpty()) return raw;
    const QDateTime lu = QDateTime::fromString(lastUsed, Qt::ISODate);
    if (!lu.isValid()) return raw;
    const double ageDays = double(lu.secsTo(QDateTime::currentDateTime())) / 86400.0;
    if (ageDays <= 0) return raw;
    const double hl = halfLifeDays(MemoryRetriever::classify(key));
    // decay toward 0.5 baseline (a memory never fully vanishes, just fades)
    return 0.5 + (raw - 0.5) * std::exp(-ageDays * std::log(2.0) / hl);
}

void MemoryConflictManager::applyDecay()
{
    // passive pass: re-save strength values at their decayed weight so the
    // ledger converges; only touches entries older than 1 day.
    const QJsonObject s = m_ledger.value("strength").toObject();
    QJsonObject out;
    for (auto it = s.begin(); it != s.end(); ++it) {
        const QString key = it.key();
        const QString lastUsed = m_ledger.value("lastUsed").toObject().value(key).toString();
        const QDateTime lu = QDateTime::fromString(lastUsed, Qt::ISODate);
        if (!lu.isValid()) { out.insert(key, it.value()); continue; }
        const double ageDays = double(lu.secsTo(QDateTime::currentDateTime())) / 86400.0;
        if (ageDays < 1.0) { out.insert(key, it.value()); continue; }
        const double raw = it.value().toDouble(1.0);
        const double hl = halfLifeDays(MemoryRetriever::classify(key));
        out.insert(key, 0.5 + (raw - 0.5) * std::exp(-ageDays * std::log(2.0) / hl));
    }
    m_ledger.insert("strength", out);
    m_dirty = true;
}

int MemoryConflictManager::usageCount(const QString &content)
{
    const QString key = normalize(content);
    return m_ledger.value("usage").toObject().value(key).toInt();
}

double MemoryConflictManager::usageFrequency(const QString &content)
{
    const int c = usageCount(content);
    // soft curve: usage / (usage + 5) -> 0..1, ~0.83 at 25 recalls
    return double(c) / (double(c) + 5.0);
}

// ---- open loops: promises / appointments to follow up ----
QList<MemoryConflictManager::OpenLoop> MemoryConflictManager::openLoops()
{
    QList<OpenLoop> out;
    const QJsonArray arr = m_ledger.value("openLoops").toArray();
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        OpenLoop l;
        l.content = o.value("content").toString();
        l.dueDate = o.value("dueDate").toString();
        l.created = o.value("created").toString();
        l.status  = o.value("status").toString("open");
        l.note    = o.value("note").toString();
        out.append(l);
    }
    return out;
}

QList<MemoryConflictManager::OpenLoop> MemoryConflictManager::dueOpenLoops()
{
    QList<OpenLoop> out;
    const QDate today = QDate::currentDate();
    for (const OpenLoop &l : openLoops()) {
        if (l.status != "open") continue;
        QDate due = QDate::fromString(l.dueDate, Qt::ISODate);
        if (!due.isValid()) {
            // no explicit due date: due after one day (next day check-in)
            due = QDate::fromString(l.created.left(10), Qt::ISODate).addDays(1);
        }
        if (due.isValid() && due <= today) out.append(l);
    }
    return out;
}

void MemoryConflictManager::addOpenLoop(const QString &content, const QString &dueDate)
{
    const QString key = content.trimmed();
    if (key.isEmpty()) return;
    // dedupe: same promise within the same week = refresh, not duplicate
    QJsonArray arr = m_ledger.value("openLoops").toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr.at(i).toObject();
        if (o.value("status").toString("open") == "open"
            && o.value("content").toString() == key) {
            o.insert("dueDate", dueDate);
            o.insert("created", QDateTime::currentDateTime().toString(Qt::ISODate));
            arr[i] = o;
            m_ledger.insert("openLoops", arr);
            m_dirty = true;
            return;
        }
    }
    QJsonObject o;
    o.insert("content", key);
    o.insert("dueDate", dueDate);
    o.insert("created", QDateTime::currentDateTime().toString(Qt::ISODate));
    o.insert("status", "open");
    arr.append(o);
    m_ledger.insert("openLoops", arr);
    m_dirty = true;
}

void MemoryConflictManager::closeOpenLoop(const QString &content, const QString &result)
{
    // direction: the user's NEW message (content) must match against the OLD
    // promise text (loop.content), not the other way around.
    QJsonArray arr = m_ledger.value("openLoops").toArray();
    bool changed = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr.at(i).toObject();
        if (o.value("status").toString("open") == "open"
            && content.contains(o.value("content").toString().mid(0, 12))) {
            o.insert("status", "closed");
            o.insert("note", result);
            arr[i] = o;
            changed = true;
        }
    }
    if (changed) {
        m_ledger.insert("openLoops", arr);
        m_dirty = true;
    }
}

// prune stale loops: closed loops keep a rolling cap of 20; open loops older
// than 30 days are archived to closed so the prompt never grows unbounded.
void MemoryConflictManager::pruneOpenLoops()
{
    QJsonArray arr = m_ledger.value("openLoops").toArray();
    QJsonArray kept;
    int closedCount = 0;
    const QDateTime now = QDateTime::currentDateTime();
    for (const QJsonValue &v : arr) {
        QJsonObject o = v.toObject();
        const QString status = o.value("status").toString("open");
        const QDateTime created = QDateTime::fromString(o.value("created").toString(), Qt::ISODate);
        if (status == "open") {
            // archive open loops older than 30 days
            if (created.isValid() && created.daysTo(now) > 30) {
                o.insert("status", "closed");
                o.insert("note", "过期未结");
            }
            kept.append(o);
        } else {
            if (closedCount < 20) {
                kept.append(o);
                closedCount++;
            }
            // older closed loops dropped
        }
    }
    m_ledger.insert("openLoops", kept);
    m_dirty = true;
}

// detect a promise/appointment in the user text, e.g. "明天面试", "周末去看房子",
// "下周交报告", "晚上记得吃药". Returns content + ISO due date (or empty = auto).
bool MemoryConflictManager::detectOpenLoop(const QString &userText, QString *contentOut, QString *dueOut)
{
    // relative day offsets; bare dayparts only count when combined with 今/明
    static const QHash<QString, int> whenMap = {
        { "明天上午", 1 }, { "明天下午", 1 }, { "明天晚上", 1 }, { "明天早上", 1 }, { "明早", 1 }, { "明晚", 1 },
        { "后天", 2 }, { "大后天", 3 },
        { "下周", 7 },
        { "明天", 1 },
        { "今晚", 0 }, { "今天", 0 },
    };
    static const QHash<QString, int> weekdayMap = {
        { "周六", 6 }, { "周日", 7 }, { "星期六", 6 }, { "星期日", 7 }, { "周天", 7 },
    };
    // outcome/action verbs that make a promise
    static const QStringList actionVerbs = {
        "去", "做", "买", "看", "写", "交", "考", "面", "打", "提", "投",
        "体检", "复查", "面试", "开会", "出差", "提交", "报名", "预约",
        "记得", "别忘了", "要交", "要考", "要面", "要打", "要去",
    };
    // bare dayparts ("晚上/下午/早上") only create a loop when combined with
    // 今/明 (e.g. "明天晚上"), which the whenMap already covers with longer
    // keys first. A bare "晚上记得吃药" is NOT a dated promise.
    if (!userText.contains("明天") && !userText.contains("今天")
        && !userText.contains("今晚") && !userText.contains("明晚")
        && !userText.contains("后天") && !userText.contains("下周")
        && !userText.contains("周") && !userText.contains("周末")) {
        // no explicit date at all — not a trackable promise
        if (userText.contains("晚上") || userText.contains("下午") || userText.contains("早上")
            || userText.contains("中午")) {
            return false;
        }
    }

    // match the LONGEST time marker first (most specific wins)
    QString when;
    QStringList candidates = whenMap.keys();
    std::sort(candidates.begin(), candidates.end(),
              [](const QString &a, const QString &b) { return a.size() > b.size(); });
    for (const QString &k : candidates) {
        if (userText.contains(k)) { when = k; break; }
    }
    if (when.isEmpty()) {
        // weekday promise ("周六去面试") -> next occurrence of that weekday
        for (auto it = weekdayMap.constBegin(); it != weekdayMap.constEnd(); ++it) {
            if (userText.contains(it.key())) {
                const int todayDow = QDate::currentDate().dayOfWeek(); // 1=Mon..7=Sun
                const int targetDow = it.value();
                int offset = (targetDow - todayDow + 7) % 7;
                if (offset == 0) offset = 7; // today's weekday -> next week
                if (contentOut) *contentOut = userText.trimmed();
                if (dueOut) *dueOut = QDate::currentDate().addDays(offset).toString(Qt::ISODate);
                return true;
            }
        }
        // "周末" -> next Saturday (or Sunday if today is Saturday)
        if (userText.contains("周末")) {
            const int todayDow = QDate::currentDate().dayOfWeek();
            int offset = (6 - todayDow + 7) % 7;
            if (offset == 0) offset = 7;
            if (contentOut) *contentOut = userText.trimmed();
            if (dueOut) *dueOut = QDate::currentDate().addDays(offset).toString(Qt::ISODate);
            return true;
        }
        return false;
    }
    // must also carry an action verb (a date alone isn't a promise)
    bool hasAction = false;
    for (const QString &v : actionVerbs)
        if (userText.contains(v)) { hasAction = true; break; }
    if (!hasAction) return false;

    QString content = userText.trimmed();
    if (content.size() > 40) content = content.left(40) + "…";
    if (contentOut) *contentOut = content;
    if (dueOut) {
        int offset = whenMap.value(when, 1);
        *dueOut = QDate::currentDate().addDays(offset).toString(Qt::ISODate);
    }
    return true;
}

// did the user report the outcome of an open loop?
bool MemoryConflictManager::detectLoopOutcome(const QString &userText)
{
    static const QStringList outcomeWords = {
        "完成了", "通过了", "过了", "黄了", "没去", "取消了", "放弃了", "没成",
        "搞定了", "成功了", "失败", "没考上", "没过", "没交", "交了", "去了",
        "面完了", "考完了", "写完了", "提交了", "报名了", "预约了", "开始搞",
    };
    for (const QString &w : outcomeWords)
        if (userText.contains(w)) return true;
    return false;
}

void MemoryConflictManager::reload()
{
    m_deprecatedCache.clear();
    m_ledger = QJsonObject();
    if (m_path.isEmpty()) return;
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        if (d.isObject()) m_ledger = d.object();
        f.close();
    }
    // warm cache from existing conflicts
    const QJsonArray conflicts = m_ledger.value("conflicts").toArray();
    for (const QJsonValue &v : conflicts) {
        QJsonObject e = v.toObject();
        if (e.value("status").toString() != "active")
            m_deprecatedCache.insert(e.value("old").toString());
    }
    m_dirty = false;
}

void MemoryConflictManager::save()
{
    if (!m_dirty || m_path.isEmpty()) return;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QJsonDocument(m_ledger).toJson());
        f.close();
        m_dirty = false;
    }
}
