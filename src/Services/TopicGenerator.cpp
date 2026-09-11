// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "TopicGenerator.h"
#include <QRegularExpression>
#include <algorithm>

void TopicGenerator::splitLine(const QString &line, QString *text, QDateTime *time, bool *hasTime)
{
    QString l = line.trimmed();
    if (l.startsWith("- ")) l = l.mid(2);
    static const QRegularExpression re("^\\[([0-9]{2}-[0-9]{2}(?:[ T][0-9]{2}:[0-9]{2})?)\\]\\s*(.*)$");
    const QRegularExpressionMatch m = re.match(l);
    if (m.hasMatch()) {
        QString stamp = m.captured(1);
        *text = m.captured(2).trimmed();
        *hasTime = true;
        QDate d = QDate::currentDate();
        if (stamp.length() >= 5) {
            const int mm = stamp.left(2).toInt(), dd = stamp.mid(3, 2).toInt();
            d = QDate(QDate::currentDate().year(), qBound(1, mm, 12), qBound(1, dd, 31));
        }
        QTime t(0, 0);
        if (stamp.contains(' ')) {
            const QString hm = stamp.section(' ', 1);
            t = QTime(qBound(0, hm.left(2).toInt(), 23), qBound(0, hm.mid(3, 2).toInt(), 59));
        }
        *time = QDateTime(d, t);
        return;
    }
    *text = l;
    *hasTime = false;
}

double TopicGenerator::totalScore(const TopicCandidate &c, double interruptPenalty)
{
    // relevance dominates; interruptCost is a penalty for barging in
    double total = c.relevance * 0.4
                 + c.freshness * 0.2
                 + c.relationship * 0.2
                 + (1.0 - c.interruptCost) * 0.2;
    return total - interruptPenalty * 0.5;
}

QList<TopicCandidate> TopicGenerator::generate(
    const QStringList &recentUserTopics,
    const QStringList &unfinished,
    const QStringList &interests,
    const QStringList &events,
    const QStringList &activityFacts,
    double interruptPenalty)
{
    QList<TopicCandidate> out;
    const QDateTime now = QDateTime::currentDateTime();

    // 1) user-recent topics: high relationship + freshness
    for (const QString &line : recentUserTopics) {
        QString text; QDateTime t; bool has;
        splitLine(line, &text, &t, &has);
        if (text.isEmpty()) continue;
        TopicCandidate c;
        c.topic = text;
        c.source = "user_recent";
        c.relevance = 0.85;
        c.freshness = has ? (t.daysTo(now) <= 1 ? 0.9 : 0.5) : 0.9;
        c.relationship = 0.85;
        c.interruptCost = 0.2;
        out << c;
    }

    // 2) unfinished topics: moderate everything
    for (const QString &line : unfinished) {
        QString text; QDateTime t; bool has;
        splitLine(line, &text, &t, &has);
        if (text.isEmpty()) continue;
        TopicCandidate c;
        c.topic = text;
        c.source = "unfinished";
        c.relevance = 0.75;
        c.freshness = has ? (t.daysTo(now) <= 2 ? 0.85 : 0.4) : 0.6;
        c.relationship = 0.7;
        c.interruptCost = 0.3;
        out << c;
    }

    // 3) real desktop activity: relevant but interrupts what they're doing
    for (const QString &line : activityFacts) {
        QString text = line.trimmed();
        if (text.isEmpty()) continue;
        TopicCandidate c;
        c.topic = text;
        c.source = "activity";
        c.relevance = 0.9;
        c.freshness = 0.9;
        c.relationship = 0.35;
        c.interruptCost = 0.8;   // barging into their current activity
        out << c;
    }

    // 4) interests: low freshness, medium relationship
    for (const QString &line : interests) {
        QString text; QDateTime t; bool has;
        splitLine(line, &text, &t, &has);
        if (text.isEmpty()) continue;
        TopicCandidate c;
        c.topic = text;
        c.source = "interest";
        c.relevance = 0.6;
        c.freshness = 0.35;
        c.relationship = 0.6;
        c.interruptCost = 0.3;
        out << c;
    }

    // 5) events (shared experiences): recent ones worth revisiting
    for (const QString &line : events) {
        QString text; QDateTime t; bool has;
        splitLine(line, &text, &t, &has);
        if (text.isEmpty()) continue;
        TopicCandidate c;
        c.topic = text;
        c.source = "event";
        c.relevance = 0.55;
        c.freshness = has ? (t.daysTo(now) <= 3 ? 0.7 : 0.3) : 0.4;
        c.relationship = 0.55;
        c.interruptCost = 0.3;
        out << c;
    }

    // score + sort desc
    for (TopicCandidate &c : out)
        c.total = totalScore(c, interruptPenalty);
    std::stable_sort(out.begin(), out.end(),
                     [](const TopicCandidate &a, const TopicCandidate &b) { return a.total > b.total; });
    while (out.size() > 10) out.removeLast();
    return out;
}
