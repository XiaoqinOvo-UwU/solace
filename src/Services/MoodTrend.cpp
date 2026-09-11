// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "MoodTrend.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>

namespace {
bool isLow(const QString &e)
{
    return e == "tired" || e == "stressed" || e == "sad" || e == "lonely" || e == "angry";
}
bool isUp(const QString &e)
{
    return e == "happy";
}
// positivity = up - low, over a sub-list
int posScore(const QStringList &list)
{
    int s = 0;
    for (const QString &e : list) {
        if (isUp(e)) s += 1;
        else if (isLow(e)) s -= 1;
    }
    return s;
}
} // namespace

void MoodTrend::load()
{
    m_recent.clear();
    if (m_path.isEmpty()) return;
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isArray()) {
            const QJsonArray arr = doc.array();
            for (const QJsonValue &v : arr) {
                const QString e = v.toString();
                if (!e.isEmpty()) m_recent << e;
            }
        }
    }
}

void MoodTrend::save()
{
    if (m_path.isEmpty()) return;
    QJsonArray arr;
    for (const QString &e : m_recent) arr.append(e);
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void MoodTrend::record(const QString &emotion)
{
    if (emotion.isEmpty()) return;
    m_recent << emotion;
    while (m_recent.size() > 10) m_recent.removeFirst();
    save();
}

QJsonObject MoodTrend::stats() const
{
    QJsonObject o;
    int happy = 0, neutral = 0, tired = 0, stressed = 0, sad = 0, lonely = 0, angry = 0;
    for (const QString &e : m_recent) {
        if (e == "happy") happy++;
        else if (e == "tired") tired++;
        else if (e == "stressed") stressed++;
        else if (e == "sad") sad++;
        else if (e == "lonely") lonely++;
        else if (e == "angry") angry++;
        else neutral++;
    }
    o.insert("happy", happy);
    o.insert("neutral", neutral);
    o.insert("tired", tired);
    o.insert("stressed", stressed);
    o.insert("sad", sad);
    o.insert("lonely", lonely);
    o.insert("angry", angry);
    return o;
}

QString MoodTrend::direction() const
{
    if (m_recent.size() < 4) return "stable";
    const int half = m_recent.size() / 2;
    const int older = posScore(m_recent.mid(0, half));
    const int newer = posScore(m_recent.mid(half));
    if (newer > older) return "up";
    if (newer < older) return "down";
    return "stable";
}

QString MoodTrend::phrasing() const
{
    const QString d = direction();
    if (d == "down")
        return "用户最近情绪略向下，关心时措辞一定要柔和，禁止直接说'你最近一直很难过/你总是不开心'；"
               "用模糊表达，例如'感觉你最近好像比之前更容易累一点'。";
    if (d == "up")
        return "用户最近情绪在变好，可以陪ta开心地多聊一些。";
    return QString();
}

QString MoodTrend::block() const
{
    const QJsonObject s = stats();
    QStringList parts;
    for (auto it = s.constBegin(); it != s.constEnd(); ++it)
        parts << QString("%1:%2").arg(it.key()).arg(it.value().toInt());
    return "【情绪趋势】（最近 10 次聊天统计，仅作语气参考，不是断言）\n"
           + parts.join("，") + "；趋势：" + direction() + (phrasing().isEmpty() ? QString() : "\n" + phrasing());
}
