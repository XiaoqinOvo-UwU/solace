// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "ConversationStateManager.h"
#include "MemoryImportanceEvaluator.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

QString ConversationStateManager::m_path;

void ConversationStateManager::setStatePath(const QString &path)
{
    m_path = path;
}

void ConversationStateManager::load(ConversationState &state)
{
    if (m_path.isEmpty()) return;
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
    QJsonDocument d = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!d.isObject()) return;
    QJsonObject o = d.object();
    state.topic             = o.value("topic").toString();
    state.userEmotion       = o.value("userEmotion").toString();
    state.assistantIntent   = o.value("assistantIntent").toString();
    state.relationshipMode  = o.value("relationshipMode").toString();
    state.unresolvedIssue   = o.value("unresolvedIssue").toString();
    state.topicSummary      = o.value("topicSummary").toString();
    state.currentMood       = o.value("currentMood").toString();
    state.interactionStyle  = o.value("interactionStyle").toString();
    state.unfinishedTopic   = o.value("unfinishedTopic").toString();
    state.lastImportantMessage = o.value("lastImportantMessage").toString();
}

void ConversationStateManager::save(const ConversationState &state)
{
    if (m_path.isEmpty()) return;
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QJsonObject o;
    o.insert("topic", state.topic);
    o.insert("userEmotion", state.userEmotion);
    o.insert("assistantIntent", state.assistantIntent);
    o.insert("relationshipMode", state.relationshipMode);
    o.insert("unresolvedIssue", state.unresolvedIssue);
    o.insert("topicSummary", state.topicSummary);
    o.insert("currentMood", state.currentMood);
    o.insert("interactionStyle", state.interactionStyle);
    o.insert("unfinishedTopic", state.unfinishedTopic);
    o.insert("lastImportantMessage", state.lastImportantMessage);
    o.insert("lastUpdate", QDateTime::currentDateTime().toString(Qt::ISODate));
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        f.write(QJsonDocument(o).toJson());
        f.close();
    }
}

void ConversationStateManager::update(ConversationState &state,
                                      const QString &userText,
                                      const QString &aiReply,
                                      const QString &emotion)
{
    Q_UNUSED(aiReply);
    const QString t = userText.trimmed();
    if (t.isEmpty()) return;

    // 1) mood — mirror the detected emotion (conversational, not diagnosis)
    if (!emotion.isEmpty())
        state.currentMood = emotion;

    // 2) interaction style — from message shape + mood
    if (emotion == "happy" || t.contains("哈哈") || t.contains("好笑") || t.contains("笑死"))
        state.interactionStyle = "playful";
    else if (emotion == "tired" || emotion == "stressed"
             || emotion == "sad" || emotion == "lonely")
        state.interactionStyle = "care";
    else if (t.size() <= 8)
        state.interactionStyle = "short";
    else if (t.size() >= 40)
        state.interactionStyle = "deep";
    else
        state.interactionStyle = "casual";

    // 3) unfinished topic — user explicitly says "let's continue later"
    static const QStringList laterMarks = {
        "下次", "改天", "回头", "待会", "等会儿", "晚点", "之后再", "明天再",
        "有空再", "先这样", "以后再",
    };
    for (const QString &m : laterMarks) {
        if (t.contains(m) && !state.topic.isEmpty()) {
            state.unfinishedTopic = state.topic.left(40);
            break;
        }
    }
    // resolved: the topic actually came back up in this conversation
    if (!state.unfinishedTopic.isEmpty() && !state.topic.isEmpty()
        && state.unfinishedTopic != state.topic
        && t.contains(state.unfinishedTopic.left(4)))
        state.unfinishedTopic.clear();

    // 4) last important message — only high-importance content is kept
    if (MemoryImportanceEvaluator::worthStoring(t))
        state.lastImportantMessage = t.left(60);

    state.lastUpdate = QDateTime::currentDateTime();
}
