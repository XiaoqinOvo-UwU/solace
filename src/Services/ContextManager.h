// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include "FactModel.h"
#include <QList>
#include <QHash>
#include <QStringList>

// =====================================================================
// ContextManager — short-term fact cache for one conversation session.
// - collects facts from user messages, system sensing, memory
// - holds hypotheses (inference) separately
// - semantic correction detection: user negates the previous AI claim
// - topic scoring for proactive chat (recency + importance + source weight)
//
// Long-term facts are NOT persisted here; they live in the existing
// memory system (notes/events). This class only caches the current
// session's view and produces prompt-ready sections.
// =====================================================================
class ContextManager
{
public:
    // ---- collection ----
    void addUserMessage(const QString &text, const QStringList &tags = {});
    void addSystemData(const QString &content, double confidence, const QStringList &tags = {});
    void addMemoryFact(const QString &content, MemoryKind kind, const QStringList &tags = {},
                       double importance = -1.0, double usage = 0.0,
                       MemoryStatus status = MemoryStatus::Active);
    void addHypothesis(const QString &content, double confidence, const QStringList &tags = {});

    void addFact(const Fact &f);                 // generic insert (dedupe by id)
    void clearSession();                         // wipe short-term cache

    // ---- memory recall (score-based, returns most relevant first) ----
    // `userMsg` = current user message, `topic` = current conversation topic.
    QList<Fact> retrieveMemories(const QString &userMsg, const QString &topic, int max = 6) const;

    // debug: human-readable recall report (for logs / tests)
    QString recallReport(const QString &userMsg, const QString &topic, int max = 6) const;

    // ---- queries ----
    const QList<Fact> &facts() const { return m_facts; }
    QList<Fact> factsBySource(FactSource s) const;
    QList<Fact> validFacts() const;              // isFact() == true
    QList<Fact> hypotheses() const;              // isHypothesis() == true
    QStringList factIds() const;

    // ---- correction ----
    // semantic negation detection: does `userText` reject a claim the AI
    // made in `lastAiReply` (or any known fact)? Returns matched fact ids.
    QStringList detectCorrection(const QString &userText, const QString &lastAiReply);
    void rejectFact(const QString &id);          // rejected=true, correctionCount++
    void rejectAllWithTag(const QString &tag);
    int  correctionCount(const QString &id) const;

    // ---- proactive topic scoring ----
    struct TopicScore {
        QString topic;
        double score = 0;
        QString source;   // where it came from
        QDateTime lastTime;
    };
    // merge unfinished-topics + interests + recent chat + events into
    // ranked topic candidates. Higher = better to talk about.
    QList<TopicScore> rankTopics(
        const QStringList &unfinishedTopics,     // e.g. "- [08-20 22:10] 我的小说"
        const QStringList &interests,            // e.g. "- Minecraft"
        const QStringList &recentChat,           // last few message lines
        const QStringList &events,               // event memory lines
        const QStringList &userRecentFacts) const; // recent user_message facts

    // ---- prompt building (used by FactFilter / AiService) ----
    QString factsSection(int maxFacts, const QString &userMsg = QString(), const QString &topic = QString()) const; // [确定] block
    QString hypothesesSection(int maxHypotheses) const; // [推测] block

    // total lifetime corrections made this session (for stats/debug)
    int totalCorrections() const { return m_totalCorrections; }

private:
    QList<Fact> m_facts;
    int m_totalCorrections = 0;
    int m_idCounter = 0;

    QString nextId(const QString &prefix);
    bool hasFactWithContent(const QString &content) const;
};
