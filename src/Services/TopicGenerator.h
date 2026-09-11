// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QList>
#include <QStringList>
#include <QDateTime>

// =====================================================================
// TopicGenerator — the proactive-chat topic engine.
//
// Builds up to 10 candidate topics from four real sources:
//   1. 用户最近主动提过的话题 (recent user topics)
//   2. 未完成事件            (unfinished topics)
//   3. 真实电脑状态          (activity facts from the desktop)
//   4. 兴趣                  (interests)
//
// Each candidate is scored on 4 dimensions:
//   relevance / freshness / relationship / interruptCost
// and the highest-scoring ones are chosen. The LLM never decides the
// candidate list — it only words the winning topic naturally.
// =====================================================================

struct TopicCandidate
{
    QString topic;
    QString source;       // "user_recent" | "unfinished" | "activity" | "interest" | "event"
    double  relevance = 0.5;
    double  freshness = 0.5;
    double  relationship = 0.5;
    double  interruptCost = 0.5;  // 1.0 = interrupting what the user is doing
    double  total = 0.0;          // weighted sum
};

class TopicGenerator
{
public:
    static QList<TopicCandidate> generate(
        const QStringList &recentUserTopics, // e.g. "- 我的小说"
        const QStringList &unfinished,       // e.g. "- [08-20 22:10] 我的小说"
        const QStringList &interests,        // e.g. "- Minecraft"
        const QStringList &events,           // event memory lines
        const QStringList &activityFacts,    // desktop-state lines (system_data)
        double interruptPenalty = 0.0);      // >0 while working/DND-ish

    // the weighted total (used internally + by tests)
    static double totalScore(const TopicCandidate &c, double interruptPenalty);

private:
    // parse "- [MM-dd HH:mm] text" or "- text" into (text, time, hasTime)
    static void splitLine(const QString &line, QString *text, QDateTime *time, bool *hasTime);
};
