// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>

// =====================================================================
// FactModel — the single source-of-truth definition for every piece of
// information the companion AI knows about the user / system.
//
// Rule: only UserMessage / SystemData / Memory facts may be treated as
// facts. Inference (Hypothesis) may be referenced but ONLY in question
// form. Rejected facts are excluded from prompts entirely.
// =====================================================================

enum class FactSource {
    UserMessage,   // user explicitly said it
    SystemData,    // real detection from the computer (foreground, processes, time...)
    Memory,        // long-term memory notes / events
    Inference      // AI guess — hypothesis only, never a fact
};

// memory sub-type — which long-term bucket this memory came from.
// Ordering (values 0..3) is stable for backward compatibility with existing
// tests / saved data. HabitMemory (4) was added in v3.9.
enum class MemoryKind {
    UserFact,      // 0 确定事实: user told the AI directly    -> confidence 1.0
    SystemData,    // 1 real detection                         -> confidence 0.95
    MemoryEvent,   // 2 历史经历: shared experience            -> confidence 0.85
    MemorySummary, // 3 AI理解: LLM auto-summary (low trust)   -> confidence 0.5
    HabitMemory    // 4 长期习惯: user-stated repeated behavior -> confidence 0.8
};

// lifecycle of a long-term memory entry (never physically deleted)
enum class MemoryStatus {
    Active,      // usable as a fact
    Deprecated,  // user contradicted it — must NOT be used as fact
    Replaced     // superseded by a newer fact (replacedBy points to it)
};

struct Fact
{
    QString     id;               // stable id, e.g. "sys_foreground_1650000000"
    QString     content;          // "用户正在玩Minecraft"
    FactSource  source = FactSource::Inference;
    MemoryKind  memKind = MemoryKind::MemorySummary; // valid when source==Memory
    double      confidence = 0.5; // 0.0 ~ 1.0
    QDateTime   timestamp = QDateTime::currentDateTime();
    bool        rejected = false;        // user corrected it -> excluded everywhere
    int         correctionCount = 0;     // how many times it was corrected
    QStringList tags;                    // e.g. {"game", "minecraft", "activity"}
    double      retrievalScore = 0.0;    // MemoryRetriever score (prompt ordering)
    double      importance = -1.0;       // MemoryImportanceEvaluator score; -1 = use kind default
    double      usageFrequency = 0.0;    // how often this memory was recalled (0..1)
    MemoryStatus status = MemoryStatus::Active; // deprecated/replaced memories are excluded
    QDateTime   lastUsedTime;            // last time this memory helped a reply

    bool isFact() const { return !rejected && status == MemoryStatus::Active && source != FactSource::Inference; }
    bool isHypothesis() const { return !rejected && source == FactSource::Inference; }

    QString memKindName() const
    {
        switch (memKind) {
        case MemoryKind::UserFact:     return "USER_FACT";
        case MemoryKind::SystemData:   return "SYSTEM_DATA";
        case MemoryKind::MemoryEvent:  return "MEMORY_EVENT";
        case MemoryKind::MemorySummary:return "INTERPRETATION";
        case MemoryKind::HabitMemory:  return "HABIT";
        }
        return "INTERPRETATION";
    }

    QString sourceName() const
    {
        switch (source) {
        case FactSource::UserMessage: return "user_message";
        case FactSource::SystemData:  return "system_data";
        case FactSource::Memory:      return "memory";
        case FactSource::Inference:   return "inference";
        }
        return "unknown";
    }

    QJsonObject toJson() const
    {
        QJsonObject o;
        o.insert("id", id);
        o.insert("content", content);
        o.insert("source", sourceName());
        o.insert("confidence", confidence);
        o.insert("timestamp", timestamp.toString(Qt::ISODate));
        o.insert("rejected", rejected);
        o.insert("corrections", correctionCount);
        o.insert("tags", QJsonArray::fromStringList(tags));
        o.insert("importance", importance);
        o.insert("usageFrequency", usageFrequency);
        o.insert("status", (int)status);
        return o;
    }
};
