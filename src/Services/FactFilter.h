// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include "FactModel.h"
#include <QStringList>

// =====================================================================
// FactFilter — code-level gate between ContextManager and Prompt Builder.
// Guarantees: only verified facts (user_message/system_data/memory) reach
// the fact block; inference never appears as a stated fact; rejected
// facts are excluded everywhere.
// =====================================================================
class FactFilter
{
public:
    // keep only facts that are allowed as stated facts
    static QList<Fact> facts(const QList<Fact> &all);
    // keep only inference entries (for the hypothesis block)
    static QList<Fact> hypotheses(const QList<Fact> &all);
    // exclude rejected + low-confidence noise
    static QList<Fact> clean(const QList<Fact> &all, double minConfidence = 0.3);

    // ---- response validation keywords ----
    // phrases that assert an observation about the user; if a reply uses
    // one of these WITHOUT matching system_data support, it is a fabrication.
    static const QStringList &observationPhrases(); // "我看到你", "你刚刚一直", "你又在", "你是不是在"...
    static const QStringList &stagePhrases();       // bracketed/starred actions handled separately

    // phrases describing physical actions the AI cannot perform (抱抱/点外卖/
    // 走到你身边...). The AI must stay in online/network communication mode.
    static const QStringList &physicalActionPhrases();

    // does the reply contain an unsupported observation phrase?
    // `supportedContent` = verified facts text; phrase must match a fact,
    // otherwise the offending part is stripped.
    static QString stripUnsupportedObservations(const QString &reply, const QString &factText);
};
