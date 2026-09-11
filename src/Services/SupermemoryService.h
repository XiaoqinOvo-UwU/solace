// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>

// =====================================================================
// SupermemoryService — long-term memory bridge to the Supermemory API.
//
// Enabled only when SUPERMEMORY_API_KEY is present in the environment;
// with no key every call is a no-op, so the app behaves exactly as
// before. All calls are blocking (nested event loop + timeout) and are
// meant to be called from a QtConcurrent worker, never the UI thread.
//
//   POST /v4/search    -> relevant extracted facts for the current turn
//   POST /v3/documents -> persist an exchange (taskType "memory")
//
// Container tag rules: one tag per end user / project, singular
// `containerTag`, pattern ^[a-zA-Z0-9_:-]+$ — defaults to "solace",
// override with SUPERMEMORY_CONTAINER_TAG.
// =====================================================================

class SupermemoryService
{
public:
    static bool enabled();

    static QString containerTag();

    // Returns "- <fact>" lines (may be empty). "" when disabled / no hit.
    static QString search(const QString &query, int limit = 6);

    // Persist one conversation exchange. Fire-and-forget.
    static void addMemory(const QString &content, const QString &customId = QString());
};
