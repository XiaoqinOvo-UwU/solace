// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "DoNotDisturbManager.h"
#include <QStringList>

DoNotDisturbState DoNotDisturbManager::evaluate(const QString &fgCategory,
                                                const QString &fgExe,
                                                bool isFullscreen,
                                                bool isGameProcess)
{
    DoNotDisturbState s;
    Q_UNUSED(fgCategory); // (kept in the signature for clarity)

    const QString e = fgExe.toLower();

    // 1) gaming — the most important silence reason
    if (isGameProcess || fgCategory == "gaming") {
        s.enabled = true;
        s.reason = "gaming";
        return s;
    }
    // 2) meeting / voice-call software
    static const QStringList meetings = {
        "zoom", "teams", "wemeet", "腾讯会议", "dingtalk", "钉钉",
        "feishu", "skype", "webex", "voov", "meeting",
    };
    for (const QString &m : meetings)
        if (e.contains(m)) {
            s.enabled = true;
            s.reason = "meeting";
            return s;
        }
    // 3) fullscreen app (any) — covers fullscreen games/videos/immersive work
    if (isFullscreen) {
        s.enabled = true;
        s.reason = "fullscreen";
        return s;
    }
    // NOTE: foreground coding/terminal no longer hard-bans. A window that stays
    // open doesn't mean the user is present; ProactiveScore's idle signal makes
    // the call instead (gaming/fullscreen/meeting still block here).

    return s; // enabled stays false
}
