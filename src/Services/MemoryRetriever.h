// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include "FactModel.h"
#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QJsonArray>
#include <QVector>

// =====================================================================
// MemoryRetriever — relevance-scored memory recall.
//
// v3.9 scoring (semantic-first):
//   score = semanticRelevance*0.35 + importance*0.30 + recency*0.15
//         + relationship*0.15 + usageFrequency*0.05
//
// The semantic layer is pluggable: when no embedding backend is installed
// the previous CJK-keyword matcher is used (default). The interface below
// exists so an embedding model can be dropped in later WITHOUT touching
// the callers (ContextManager / AiService).
//
// MemoryKind is defined in FactModel.h — this module reuses it.
// =====================================================================

// optional embedding backend — plug in a real model later, keyword fallback otherwise
class SemanticEmbedder
{
public:
    virtual ~SemanticEmbedder() = default;
    // vector embedding of `text` (empty vector = not available for this text)
    virtual QVector<float> embed(const QString &text) = 0;
    // similarity in [0,1] between two texts (uses embed internally)
    virtual float similarity(const QString &a, const QString &b) = 0;
};

struct MemoryEntry
{
    QString     content;
    QString     kindName;       // "USER_FACT" | "SYSTEM_DATA" | "MEMORY_EVENT" | "INTERPRETATION" | "HABIT"
    double      importance = 0.5;    // MemoryImportanceEvaluator score
    double      confidence = 0.5;
    double      score = 0.0;         // computed by MemoryRetriever
    double      usageFrequency = 0.0;
    QDateTime   createdTime;
    QDateTime   lastUsedTime;
    MemoryStatus status = MemoryStatus::Active;
    QStringList tags;
    bool        rejected = false;
    bool        isEvent = false;     // true if it came from events[] (shared experience)

    static QString kindLabel(MemoryKind k)
    {
        switch (k) {
        case MemoryKind::UserFact:     return QStringLiteral("USER_FACT");
        case MemoryKind::SystemData:   return QStringLiteral("SYSTEM_DATA");
        case MemoryKind::MemoryEvent:  return QStringLiteral("MEMORY_EVENT");
        case MemoryKind::MemorySummary:return QStringLiteral("INTERPRETATION");
        case MemoryKind::HabitMemory:  return QStringLiteral("HABIT");
        }
        return QStringLiteral("INTERPRETATION");
    }
    static double kindConfidence(MemoryKind k)
    {
        switch (k) {
        case MemoryKind::UserFact:     return 1.0;
        case MemoryKind::SystemData:   return 0.95;
        case MemoryKind::MemoryEvent:  return 0.85;
        case MemoryKind::MemorySummary:return 0.5;
        case MemoryKind::HabitMemory:  return 0.8;
        }
        return 0.5;
    }
    static double kindImportance(MemoryKind k)
    {
        switch (k) {
        case MemoryKind::UserFact:     return 1.0;
        case MemoryKind::SystemData:   return 0.85;
        case MemoryKind::MemoryEvent:  return 0.75;
        case MemoryKind::MemorySummary:return 0.4;
        case MemoryKind::HabitMemory:  return 0.9;
        }
        return 0.4;
    }
};

class MemoryRetriever
{
public:
    // classify a raw note string into a MemoryKind
    static MemoryKind classify(const QString &note);

    // ---- semantic layer ----
    static void setEmbedder(SemanticEmbedder *embedder); // takes ownership? no — raw pointer, not owned
    static SemanticEmbedder *embedder();

    // keyword-based semantic relevance (default until an embedder is set)
    static double relevanceScore(const QString &memoryContent, const QString &userMsg, const QString &topic);

    // full score (v3.9 weights):
    //   semanticRelevance*0.35 + importance*0.30 + recency*0.15
    // + relationship*0.15 + usageFrequency*0.05
    static double score(const QString &content, MemoryKind kind,
                        const QDateTime &ts, bool isEvent,
                        const QString &userMsg, const QString &topic,
                        double usageFrequency = 0.0,
                        double importanceOverride = -1.0);

    // tag overlap / keyword helper used by relevance
    static bool sharesTerms(const QString &a, const QString &b);
};
