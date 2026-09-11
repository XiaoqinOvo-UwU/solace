// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>

// =====================================================================
// ResponseRepair — rewrites a fabricated observation ("我看到你..."/"你又…")
// into a natural, uncertain, persona-preserving expression instead of just
// deleting it. The AI keeps its warmth but stops asserting unverified facts.
//
//   before: 我看到你又熬夜了。
//   after:  感觉你最近好像挺晚睡的，要注意休息哦。
//
// Only used when the observation has NO system_data support. Supported
// observations pass through untouched.
// =====================================================================
class ResponseRepair
{
public:
    // does this line contain an observation phrase with no data behind it?
    static bool hasUnsupportedObservation(const QString &line);

    // returns a repaired line (non-empty) or empty when no repair is possible
    static QString repairObservationLine(const QString &line);
};
