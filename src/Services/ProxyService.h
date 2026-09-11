// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>

// Ported from XiaoQinV2rayHelper: proxy detection, launch, import.
class ProxyService : public QObject
{
    Q_OBJECT
public:
    explicit ProxyService(QObject *parent = nullptr);

    Q_INVOKABLE bool isClashRunning();
    Q_INVOKABLE bool isV2rayRunning();
    Q_INVOKABLE bool isClashPortOpen();   // 7897
    Q_INVOKABLE bool isV2rayPortOpen();   // 10808

    Q_INVOKABLE bool launchClash();
    Q_INVOKABLE bool launchV2ray();
    Q_INVOKABLE bool launchAny();         // open whichever is installed
    Q_INVOKABLE bool hasClash();          // is Clash installed
    Q_INVOKABLE bool hasV2ray();          // is v2rayN installed

    Q_INVOKABLE void copyToClipboard(const QString &text);

    // fast status (port-only, async via worker thread)
    Q_INVOKABLE QString quickStatus();
    Q_INVOKABLE void statusAsync();

    Q_INVOKABLE QString clashExe();
    Q_INVOKABLE QString v2rayExe();

signals:
    void statusReady(QString status);

private:
    bool processRunning(const QString &name);
    bool portOpen(int port);
};
