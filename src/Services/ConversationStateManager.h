// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include "ConversationState.h"
#include <QString>

// =====================================================================
// ConversationStateManager — persists and updates the short-term
// conversation state so continuity survives app restarts.
//
// File: conversation_state.json (contact dir, sibling of memory.json).
// memory.json is never touched. The struct itself stays session-only in
// spirit: the copy on disk is just a "resume point", not long-term memory.
// =====================================================================
class ConversationStateManager
{
public:
    // call once from AiService ctor
    static void setStatePath(const QString &path);

    // load a previous session's resume point into `state`
    static void load(ConversationState &state);

    // persist `state` (no-op if allowLongTermMemory is off — caller checks)
    static void save(const ConversationState &state);

    // refresh the v3.9 continuity fields (mood/style/unfinished/important)
    static void update(ConversationState &state,
                       const QString &userText,
                       const QString &aiReply,
                       const QString &emotion);

private:
    static QString m_path;
};
