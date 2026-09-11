// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// Ported from XiaoQinV2rayHelper: junk cleanup, memory cleanup, startup items.
class SystemService : public QObject
{
    Q_OBJECT
public:
    explicit SystemService(QObject *parent = nullptr);

    Q_INVOKABLE QString cleanJunk();     // returns freed MB
    Q_INVOKABLE QString cleanMemory();   // message string
    Q_INVOKABLE QString listStartupItems(); // newline-separated report
    Q_INVOKABLE QStringList startupItemList(); // "name|command|source" per item
    Q_INVOKABLE bool disableStartupItem(const QString &name);
    Q_INVOKABLE bool enableStartupItem(const QString &name);

    // async cleanup: heavy file deletion runs in background, reports via signal
    Q_INVOKABLE void cleanJunkAsync();

    // extra tools
    Q_INVOKABLE QString scanLargeFiles();      // top large files on C:

    // app auto-start (开机自启) via HKCU Run key
    Q_INVOKABLE bool setAutoStart(bool enabled);
    Q_INVOKABLE bool isAutoStartEnabled();

signals:
    void cleanupDone(QString message);
};
