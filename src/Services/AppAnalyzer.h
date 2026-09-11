// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>

// =====================================================================
// AppAnalyzer — converts RAW desktop data (process exe, window title,
// foreground minutes) into structured, LLM-safe semantic facts.
//
// 禁止 LLM 自己解释进程：the model only ever sees this layer's OUTPUT
// (content / category / source / confidence). Raw exe names never reach
// the prompt for interpretation.
//
// Browser content is EXPLICITLY not inferred — "用户正在使用浏览器"
// and nothing more about what's being browsed.
//
// isExcluded() marks proxies / accelerators / file explorer / system
// chrome as non-activity (never counted as "what the user was doing").
// =====================================================================

struct AppAnalysis
{
    QString content;     // "用户正在玩 Minecraft"
    QString category;    // gaming | creating | browsing | chatting | media | document | terminal | other | noise
    QString source;      // always "system_data"
    double  confidence;  // 0.0 ~ 1.0
    bool    isSystemNoise = false; // proxies/explorer/system -> not a user activity
};

class AppAnalyzer
{
public:
    // main entry: analyze a foreground app into a semantic fact.
    // `exe` = process file name (e.g. "Minecraft.exe"), `title` = window title,
    // `minutes` = foreground time so far (0 = unknown).
    static AppAnalysis analyze(const QString &exe, const QString &title, int minutes);

    // true for proxies/accelerators/explorer/system chrome (never a user activity)
    static bool isExcluded(const QString &exe, const QString &title);

    // friendly human label for a process (e.g. "Minecraft", "代码编辑器")
    static QString appLabel(const QString &exe, const QString &title);
};
