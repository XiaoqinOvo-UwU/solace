// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>

// =====================================================================
// ResponseValidator — mandatory post-processing gate for ALL LLM output
// (sendMessage / idleChat / greeting / summarizer). Nothing reaches the
// UI without passing through here.
//
// Checks:
//  1. strip <think> blocks and control tokens with or without a payload:
//     <|ACT|>, <|ACT {...}|>, <|THINK|>, <|THINK {...}|>, <|DELAY|>, <|DELAY 1.5|>
//     plus a final safety net that drops ANY residual <|...|> block.
//  2. strip stage directions: （动作）、*动作*、standalone narration lines
//  3. unsupported fabricated observations ("我看到你..." without data) are
//     REPAIRED by ResponseRepair into natural uncertain expressions rather
//     than simply deleted (deletion remains as a last-resort fallback)
//  4. enforce dialog-only output (no role scripts, no narration)
//
// Cleanup order (mandatory):
//   raw -> stripControlTokens -> stripStageDirections -> ResponseRepair
//       -> dialog-only -> final safety filter -> speech
// =====================================================================
class ResponseValidator
{
public:
    struct Result {
        QString text;       // cleaned output
        bool    changed;    // true if anything was stripped
        int     stagedStripped = 0;
        int     observationsStripped = 0;
        int     observationsRepaired = 0;
        int     guiltStripped = 0;       // v4.3: manipulative pressure lines dropped
        int     driftRepaired = 0;       // v4.3: persona-drift phrases neutralized
    };

    // main entry: `verifiedFacts` = the [确定] fact block text used in the
    // prompt (used to decide whether an observation is supported).
    static Result validate(const QString &raw, const QString &verifiedFacts);

    // v4.3 outbound guards (public for reuse/tests)
    static QString stripGuiltPressure(const QString &s, int *countOut = nullptr); // 愧疚/施压话术硬拦截
    static QString neutralizeDrift(const QString &s, int *countOut = nullptr);    // 人设漂移话术改写

    // lightweight helpers (public for reuse/tests)
    static QString stripControlTokens(const QString &raw);   // think/ACT/DELAY
    static QString stripStageDirections(const QString &raw); // (xx) *xx* narration
    static QString stripUnsupportedObservations(const QString &raw, const QString &factText);
    static QString trimToDialog(const QString &raw);         // drop quote-less narration lines
    static QString stripPhysicalActions(const QString &raw); // online-mode: drop body-action lines

private:
    static QString stripUnsupportedObservationsImpl(const QString &raw, const QString &factText,
                                                    int *repaired, int *dropped);
};
