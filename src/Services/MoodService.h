// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// Mood diary backed by a simple text file (ported from WinForms).
// Moods are the cute option set the user chose.
class MoodService : public QObject
{
    Q_OBJECT
public:
    explicit MoodService(QObject *parent = nullptr);

    Q_INVOKABLE QStringList moodOptions() const;
    Q_INVOKABLE bool recordMood(const QString &mood);       // append with timestamp
    Q_INVOKABLE QString history();                          // full history text
    QString diaryPath() const;
};
