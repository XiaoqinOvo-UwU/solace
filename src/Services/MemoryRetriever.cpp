// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "MemoryRetriever.h"
#include "MemoryConflictManager.h"
#include <QSet>
#include <QStringList>
#include <QRegularExpression>
#include <QTime>
#include <cmath>

namespace {
SemanticEmbedder *g_embedder = nullptr;
}

void MemoryRetriever::setEmbedder(SemanticEmbedder *embedder)
{
    g_embedder = embedder;
}

SemanticEmbedder *MemoryRetriever::embedder()
{
    return g_embedder;
}

// classify a raw memory note into a kind.
// Heuristics: auto-summaries usually contain time patterns like （yyyy-MM-dd） or
// "休息了约 N 小时"; shared-experience events are tagged separately by caller.
MemoryKind MemoryRetriever::classify(const QString &note)
{
    const QString n = note.trimmed();
    // LLM auto-summaries: machine-generated meta-notes (NOT plain timestamps —
    // user notes also carry （date time） suffixes and must stay classified by content)
    static const QRegularExpression autoRe("休息了约|本次开机|上次会话结束|自动生成|系统记录|每日摘要");
    if (autoRe.match(n).hasMatch())
        return MemoryKind::MemorySummary; // AI理解
    // explicit habits: repeated behavior patterns
    static const QStringList habitMarks = { "经常", "习惯", "每天晚上", "每天早上", "每天",
                                            "每次", "平时", "日常", "作息", "一到" };
    for (const QString &m : habitMarks)
        if (n.contains(m)) return MemoryKind::HabitMemory;
    // explicit user statements: first-person-ish or direct user descriptions
    static const QStringList userMarks = { "用户喜欢", "用户使用", "用户有", "用户是",
                                           "我喜欢", "我是", "用户常", "用户习惯", "用户作息" };
    for (const QString &m : userMarks)
        if (n.contains(m)) return MemoryKind::UserFact;
    // user statements about their own current state — covers conflict-created
    // replacement facts like "用户现在不玩Apex了" (explicitly user-said)
    static const QStringList stateVerbs = {
        "不玩", "不喝", "不打了", "不看了", "不再", "不追", "不喜欢",
        "不写了", "不用了", "没在玩", "退坑", "卸载", "在玩", "开始玩", "现在只玩",
    };
    if (n.startsWith("用户")) {
        for (const QString &v : stateVerbs)
            if (n.contains(v)) return MemoryKind::UserFact;
    }
    // fall back: plain notes with no strong signal stay an AI-interpretation
    return MemoryKind::MemorySummary;
}

bool MemoryRetriever::sharesTerms(const QString &a, const QString &b)
{
    if (a.isEmpty() || b.isEmpty()) return false;
    // direct substring is the strongest signal
    if (a.contains(b) || b.contains(a)) return true;
    // bound input to avoid O(n^2) blowups on long messages
    const QString sa = a.simplified().left(120);
    const QString sb = b.simplified().left(120);
    if (sa.size() < 2 || sb.size() < 2) return false;
    // shared CJK bigrams via hash set (approx tokenization for Chinese)
    QSet<QString> bigA;
    for (int i = 0; i + 1 < sa.size(); ++i) {
        const QChar c0 = sa.at(i), c1 = sa.at(i + 1);
        if (c0.unicode() > 0x2E80 && c1.unicode() > 0x2E80)
            bigA.insert(QString(c0) + c1);
    }
    if (bigA.isEmpty()) return false;
    int hits = 0;
    for (int i = 0; i + 1 < sb.size(); ++i) {
        const QChar c0 = sb.at(i), c1 = sb.at(i + 1);
        if (c0.unicode() > 0x2E80 && c1.unicode() > 0x2E80) {
            if (bigA.contains(QString(c0) + c1)) {
                if (++hits >= 2) return true;
            }
        }
    }
    return false;
}

double MemoryRetriever::relevanceScore(const QString &memoryContent, const QString &userMsg, const QString &topic)
{
    double s = 0.0;
    if (sharesTerms(memoryContent, userMsg)) s += 0.6;
    if (!topic.isEmpty() && sharesTerms(memoryContent, topic)) s += 0.4;
    // time-of-day heuristic: memories mentioning night/hour match late chats
    if (memoryContent.contains("深夜") || memoryContent.contains("凌晨")) {
        int h = QTime::currentTime().hour();
        if (h >= 22 || h < 6) s += 0.25;
    }
    return qBound(0.0, s, 1.0);
}

// semantic relevance — uses the embedding backend if installed, otherwise
// the CJK keyword matcher. Callers are unaware of which one is active.
static double semanticRelevance(const QString &content, const QString &userMsg, const QString &topic)
{
    SemanticEmbedder *emb = MemoryRetriever::embedder();
    if (emb) {
        const QString query = userMsg.isEmpty() ? topic : userMsg;
        if (!query.isEmpty())
            return qBound(0.0, (double)emb->similarity(content, query), 1.0);
    }
    return MemoryRetriever::relevanceScore(content, userMsg, topic);
}

double MemoryRetriever::score(const QString &content, MemoryKind kind,
                              const QDateTime &ts, bool isEvent,
                              const QString &userMsg, const QString &topic,
                              double usageFrequency, double importanceOverride)
{
    const double semantic = semanticRelevance(content, userMsg, topic);
    const double importance = importanceOverride >= 0.0
                                  ? qBound(0.0, importanceOverride, 1.0)
                                  : MemoryEntry::kindImportance(kind);
    // recency: decay over ~14 days
    const qint64 ageDays = ts.isValid() ? qMax<qint64>(0, ts.daysTo(QDateTime::currentDateTime())) : 7;
    const double recency = qBound(0.0, 1.0 - ageDays / 14.0, 1.0);
    // relationship: shared experiences with the AI weigh more
    const double relationship = isEvent ? 0.8 : 0.4;
    const double usage = qBound(0.0, usageFrequency, 1.0);

    // v4.3 memory lifecycle: effective strength (reinforcement x forgetting
    // curve) nudges the score ±0.15 so recalled-again memories rank higher
    // and long-unused ones fade out of recall naturally.
    const double strengthBonus = qBound(-0.15, (MemoryConflictManager::effectiveStrength(content) - 1.0) * 0.15, 0.15);

    return semantic * 0.35 + importance * 0.30 + recency * 0.15
           + relationship * 0.15 + usage * 0.05 + strengthBonus;
}
