// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "ProactiveScore.h"

int ProactiveScore::compute(const ProactiveInput &in)
{
    int score = 0;

    // ---- positive ---- (idle is the dominant signal: no user input = safe to reach out)
    if (in.idleMs >= 10 * 60 * 1000)   score += 30; // short idle
    if (in.idleMs >= 30 * 60 * 1000)   score += 35; // long idle (cumulative -> 65)
    if (in.justFinishedTask)            score += 15;
    if (in.lateNight && !in.gaming)     score += 15; // online late at night
    if (in.userMoodLow)                 score += 15;

    // ---- negative ----
    // a foreground editor/terminal no longer blocks by itself: if the user has
    // actually been idle for a while they likely stepped away, so reaching out
    // is welcome. Gaming / fullscreen / refusal stay strong blockers.
    if (in.gaming)                      score -= 70;
    if (in.coding)                      score -= 15;
    if (in.justRefused)                 score -= 30;
    if (in.fullscreen)                  score -= 30;

    return score;
}
