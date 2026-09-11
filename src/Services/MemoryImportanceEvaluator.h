// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>

// =====================================================================
// MemoryImportanceEvaluator — decides whether a piece of chat content is
// worth entering long-term memory.
//
// Worth storing:
//   - explicit long-term preferences  ("我喜欢Minecraft")
//   - habits                          ("我经常晚上写代码")
//   - important shared experiences    ("你之前帮我解决了服务器问题")
//   - explicit remember requests      ("记住……")
//   - explicit updates/negations      ("我现在不玩Apex了") — must be kept
//     so the conflict manager can deprecate the old fact
//
// NOT worth storing:
//   - small talk / one-off questions  ("今天吃了吗")
//   - transient states                ("我刚洗了个澡")
//   - plain acknowledgements          ("嗯", "哈哈", "好")
// =====================================================================

enum class ImportanceLevel {
    Noise,   // never store
    Low,     // store only if explicitly requested
    Medium,  // borderline — keep if it repeats
    High     // definitely store
};

class MemoryImportanceEvaluator
{
public:
    // 0.0 ~ 1.0 continuous importance score
    static double importanceScore(const QString &text);

    static ImportanceLevel level(const QString &text);

    // true if importance >= Medium threshold (0.5)
    static bool worthStoring(const QString &text);

    static QString levelName(ImportanceLevel l);
};
