// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QSet>
#include <QDateTime>
#include "FactModel.h"

// =====================================================================
// MemoryConflictManager — resolves contradictions between old long-term
// memory and new user statements.
//
// Example:
//   old: 用户喜欢Apex
//   new: 我现在不玩Apex了
//   -> the old fact is NOT deleted from memory.json (kept for history),
//      but marked deprecated in a sidecar ledger so it can never be used
//      as a fact in prompts again. A replacement fact may be created.
//
// Ledger file: memory_meta.json (sibling of memory.json in the contact
// dir). memory.json stays byte-compatible — zero migration needed.
//
// Ledger shape:
//   {
//     "conflicts": [
//       { "old": "<note content>", "status": "deprecated",
//         "reason": "用户主动更新", "replacedBy": "<new content>",
//         "time": "ISO" }
//     ],
//     "usage":   { "<note content>": 5 },
//     "lastUsed":{ "<note content>": "ISO" }
//   }
// =====================================================================

class MemoryConflictManager
{
public:
    // sidecar path (call once from AiService ctor)
    static void setLedgerPath(const QString &path);
    static QString ledgerPath();

    struct Resolution {
        bool hadConflict = false;
        QStringList deprecated;   // old note contents (exact memory.json strings)
        QStringList created;      // new fact contents to store as active notes
    };

    // ---- conflict detection ----
    // `existingNotes` = exact strings currently in memory.json notes[].
    // Deprecates matching old notes, and returns new facts to persist.
    static Resolution resolve(const QString &userText, const QStringList &existingNotes);

    // ---- query ----
    static bool isDeprecated(const QString &content);   // normalized
    static QSet<QString> deprecatedSet();               // cached normalized contents
    static QString replacementFor(const QString &content); // "" if none

    // ---- usage tracking (retrieval frequency) ----
    static void bumpUsage(const QString &content);
    static int usageCount(const QString &content);
    static double usageFrequency(const QString &content); // 0.0 ~ 1.0 normalized

    // ---- memory lifecycle (v4.3): reinforcement + forgetting curve ----
    // strength starts at 1.0; recall strengthens (+0.1, cap 2.0), time decays
    // it toward the kind half-life baseline. Decay never deletes data — it
    // only lowers the effective weight so old/less-relevant memories fade
    // out of recall (like people).
    static void   strengthen(const QString &content);            // called by bumpUsage
    static double strength(const QString &content);              // raw ledger value
    static double effectiveStrength(const QString &content);     // strength x decay
    static void   applyDecay();                                  // passive decay pass
    static double halfLifeDays(MemoryKind kind);                 // 90/60/30/14

    // ---- open loops (v4.3): promises / appointments to follow up ----
    // "明天面试" -> an open loop the AI can ask about when it's due.
    struct OpenLoop {
        QString content;     // what was promised ("明天面试")
        QString dueDate;     // ISO date when it should be asked about (empty = auto)
        QString created;     // ISO datetime
        QString status;      // "open" | "closed"
        QString note;        // result if closed ("通过了")
    };
    // returns loops that are open AND due (dueDate <= today, or auto after 1 day)
    static QList<OpenLoop> dueOpenLoops();
    static QList<OpenLoop> openLoops();
    static void addOpenLoop(const QString &content, const QString &dueDate);
    static void closeOpenLoop(const QString &content, const QString &result);
    static void pruneOpenLoops();                               // cap + archive stale loops
    static bool detectOpenLoop(const QString &userText, QString *contentOut, QString *dueOut);
    static bool detectLoopOutcome(const QString &userText);      // "完成了/黄了/通过了..."

    // normalize a note string: strip trailing "（yyyy-MM-dd HH:mm）" suffix
    // and whitespace so the same fact written on different days matches.
    static QString normalize(const QString &content);

    static void reload();
    static void save();

private:
    static void recordDeprecation(const QString &oldNote, const QString &reason,
                                  const QString &replacedBy);
    static QJsonObject ledger();               // cached copy (empty = not loaded)
    static QString reasonForUpdate(const QString &userText);
    static bool containsUpdateSignal(const QString &userText);

    static QString m_path;
    static QJsonObject m_ledger;
    static QSet<QString> m_deprecatedCache;
    static bool m_dirty;
};
