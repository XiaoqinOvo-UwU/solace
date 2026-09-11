// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QDateTime>

// =====================================================================
// RelationshipState — the CURRENT relationship context, persisted in
// relationship_state.json (sibling of relationship.json).
//
//   tone            : "warm" | "quiet" | "playful"
//   currentMood     : latest user emotion reading
//   unfinishedTopic : an unresolved topic the AI must keep in mind
//   lastInteraction : when the user last talked
//
// Purpose: after a heavy/emotional chat the AI should NOT pivot to a
// random cheerful topic ("今天幸运值怎么样？"). block() injects the
// current state so the reply continues appropriately.
// =====================================================================

class RelationshipState
{
public:
    void setPath(const QString &p) { m_path = p; load(); }

    void update(const QString &emotion, const QString &topic, const QString &userText);
    QString block() const;      // prompt-ready state section
    QString raw() const;        // debug / tests

    QString tone() const { return m_tone; }
    QString currentMood() const { return m_mood; }
    QString unfinishedTopic() const { return m_unfinishedTopic; }

private:
    void load();
    void save();
    QString m_path;
    QString m_tone = "warm";
    QString m_mood;
    QString m_unfinishedTopic;
    QDateTime m_lastInteraction;
};
