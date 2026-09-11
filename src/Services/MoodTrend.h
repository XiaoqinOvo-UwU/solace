// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QStringList>
#include <QJsonObject>

// =====================================================================
// MoodTrend — emotion trend over the last 10 chats.
//
//   stats()    -> { happy:2, neutral:3, tired:5 }   (last 10)
//   direction()-> "up" | "stable" | "down"          (first half vs second half)
//   phrasing() -> a SOFT wording constraint for the prompt.
//
// AI must NOT say "你最近一直很难过" — use fuzzy phrasing instead
// ("感觉你最近好像比之前更容易累一点").
//
// Persisted so the trend survives restarts (10 chats across sessions).
// =====================================================================

class MoodTrend
{
public:
    void setPath(const QString &p) { m_path = p; load(); }

    void record(const QString &emotion);      // happy/sad/tired/stressed/lonely/angry/normal
    QJsonObject stats() const;                // counts over the window
    QString direction() const;                // up | stable | down
    QString phrasing() const;                 // prompt hint ("" = nothing to say)
    QString block() const;                    // prompt-ready trend block

    QStringList recent() const { return m_recent; } // for tests

private:
    void load();
    void save();
    QString m_path;
    QStringList m_recent;                     // last 10 emotions
};
