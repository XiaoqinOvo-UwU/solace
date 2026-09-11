// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>

// =====================================================================
// ProactiveScore — score-based decision of whether the AI should start a
// conversation. The LLM does NOT decide this; a deterministic formula does.
//
// Positive:
//   +30 user has been inactive for a long time
//   +20 user just finished a task
//   +20 late night and the user is online
//   +15 user mood is low
// Negative:
//   -50 user is gaming
//   -40 user is coding
//   -30 user recently refused a chat
//   -20 user is in a fullscreen app
//
//  below 45 -> do NOT proactively chat
// =====================================================================

struct ProactiveInput
{
    qint64 idleMs = 0;        // ms since last keyboard/mouse input
    bool   lateNight = false; // 23:00 - 05:00
    bool   userMoodLow = false; // recent emotion low (tired/stressed/sad/lonely)
    bool   gaming = false;    // currently playing a game
    bool   coding = false;    // currently coding / creating / terminal
    bool   justRefused = false; // user recently declined a chat
    bool   fullscreen = false;
    bool   justFinishedTask = false; // just switched out of work/gaming
};

class ProactiveScore
{
public:
    static int compute(const ProactiveInput &in);

    // the gate: below 45 -> stay quiet
    static bool shouldProactivelyChat(const ProactiveInput &in) { return compute(in) >= 45; }
};
