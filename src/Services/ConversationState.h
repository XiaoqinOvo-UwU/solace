// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QStringList>
#include <QDateTime>

// =====================================================================
// ConversationState — short-term session memory that keeps the AI on the
// current topic and emotional track across turns. Lives ONLY for the
// current chat session; never written to long-term memory (memory.json).
// A lightweight copy is persisted by ConversationStateManager so a new
// session can continue where the last one stopped.
//
// State fields:
//   topic             — what we are talking about right now
//   userEmotion       — detected emotion of the user this turn
//   assistantIntent   — what the AI decided to do (comfort/ask/listen/...)
//   relationshipMode  — comfort/support vs casual vs guidance
//   unresolvedIssue   — an emotional event still needing attention
//   topicSummary      — mid-term condensed summary (filled by maybeSummarize)
//   currentMood       — perceived mood (same source as userEmotion)
//   interactionStyle  — casual/deep/short/playful/care
//   unfinishedTopic   — a topic the user explicitly wants to return to
//   lastImportantMessage — the last high-importance thing the user said
//
// NOTE: this is conversational state, NOT a psychological diagnosis.
// =====================================================================
struct ConversationState
{
    QString topic;               // "用户感到被忽视"
    QString userEmotion;         // happy/sad/lonely/stressed/tired/normal/...
    QString assistantIntent;     // "安慰并确认关系"
    QString relationshipMode;    // "comfort" | "casual" | "guidance" | "neutral"
    QString unresolvedIssue;     // non-empty while an emotional issue is open
    QString topicSummary;        // mid-term: condensed summary of the current topic

    // v3.9 conversation-continuity fields
    QString currentMood;         // AI-perceived mood this turn
    QString interactionStyle;    // casual / deep / short / playful / care
    QString unfinishedTopic;     // topic the user wants to continue later
    QString lastImportantMessage;

    QDateTime lastUpdate;

    void clear() { *this = ConversationState(); }

    bool hasUnresolvedEmotion() const
    {
        return !unresolvedIssue.isEmpty();
    }

    // prompt-ready summary (Chinese, compact)
    QString summary() const
    {
        QStringList parts;
        if (!topic.isEmpty())
            parts << "当前话题：" + topic;
        if (!userEmotion.isEmpty())
            parts << "用户情绪：" + userEmotion;
        if (!assistantIntent.isEmpty())
            parts << "你正在做的事：" + assistantIntent;
        if (!relationshipMode.isEmpty())
            parts << "当前关系模式：" + relationshipMode;
        if (!interactionStyle.isEmpty())
            parts << "交互节奏：" + interactionStyle;
        if (!unfinishedTopic.isEmpty())
            parts << "用户提到稍后继续：" + unfinishedTopic;
        if (!lastImportantMessage.isEmpty())
            parts << "用户最近说的重要的事：" + lastImportantMessage;
        if (hasUnresolvedEmotion())
            parts << "未解决的情绪事件：" + unresolvedIssue;
        return parts.join("\n");
    }
};
