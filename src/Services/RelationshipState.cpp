// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "RelationshipState.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>

void RelationshipState::load()
{
    if (m_path.isEmpty()) return;
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) {
            const QJsonObject o = doc.object();
            m_tone = o.value("tone").toString(m_tone);
            m_mood = o.value("currentMood").toString();
            m_unfinishedTopic = o.value("unfinishedTopic").toString();
            m_lastInteraction = QDateTime::fromString(o.value("lastInteraction").toString(), Qt::ISODate);
        }
    }
}

void RelationshipState::save()
{
    if (m_path.isEmpty()) return;
    QJsonObject o;
    o.insert("tone", m_tone);
    o.insert("currentMood", m_mood);
    o.insert("unfinishedTopic", m_unfinishedTopic);
    o.insert("lastInteraction", m_lastInteraction.toString(Qt::ISODate));
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

void RelationshipState::update(const QString &emotion, const QString &topic, const QString &userText)
{
    m_mood = emotion;
    m_lastInteraction = QDateTime::currentDateTime();

    // tone follows the emotion (gentle when down, playful when up)
    if (emotion == "happy") m_tone = "playful";
    else if (emotion == "sad" || emotion == "stressed" || emotion == "lonely" || emotion == "tired")
        m_tone = "quiet";
    else m_tone = "warm";

    // unfinished-topic bookkeeping: raised when the user asks/emotes about
    // something; cleared when they signal it's resolved.
    const QString t = userText.trimmed();
    const bool asks = t.endsWith(QStringLiteral("?")) || t.endsWith(QStringLiteral("？"))
                      || t.contains("怎么办") || t.contains("帮我")
                      || t.contains("不想") || t.contains("烦") || t.contains("累") || t.contains("难过");
    const bool resolved = t.contains("没事") || t.contains("好啦") || t.contains("好了")
                          || t.contains("知道了") || t.contains("可以了") || t.contains("谢谢你");
    if (asks && !topic.isEmpty())
        m_unfinishedTopic = topic;
    else if (resolved)
        m_unfinishedTopic.clear();

    save();
}

QString RelationshipState::block() const
{
    QString s = "【关系状态】（当前互动语境，延续而非跳转）\n";
    s += "语气：" + m_tone + "；最近情绪：" + (m_mood.isEmpty() ? "无记录" : m_mood);
    if (!m_unfinishedTopic.isEmpty())
        s += "；未完成话题：" + m_unfinishedTopic + "（先回应它，不要跳到无关的问候/闲聊）";
    if (m_lastInteraction.isValid())
        s += "；上次互动：" + m_lastInteraction.toString("MM-dd HH:mm");
    return s;
}

QString RelationshipState::raw() const
{
    QJsonObject o;
    o.insert("tone", m_tone);
    o.insert("currentMood", m_mood);
    o.insert("unfinishedTopic", m_unfinishedTopic);
    o.insert("lastInteraction", m_lastInteraction.toString(Qt::ISODate));
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Indented));
}
