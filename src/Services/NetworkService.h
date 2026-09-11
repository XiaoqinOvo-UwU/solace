// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>

class QNetworkAccessManager;

// Ported from XiaoQinV2rayHelper: node speed test, network speed test, proxy self-diagnosis.
// All slow operations run on a background thread and report back via signals,
// so the QML/UI thread never blocks.
class NetworkService : public QObject
{
    Q_OBJECT
public:
    explicit NetworkService(QObject *parent = nullptr);

    // synchronous core (used internally on background thread)
    static QString pingHostSync(const QString &host, int timeoutMs = 3000);
    static QString downloadSpeedMbpsSync(bool viaProxy, int bytes = 10 * 1024 * 1024);
    static QString diagnoseProxySync();

    // async QML-facing API: returns immediately, emits finished signal
    Q_INVOKABLE void pingAsync(const QString &host);
    Q_INVOKABLE void speedTestAsync(bool viaProxy, int bytes);
    Q_INVOKABLE void diagnoseAsync();
    Q_INVOKABLE void nodeTestAsync();

    // generic async JSON GET (home page: GitHub trending / latest release).
    // returns immediately; emits jsonReady(tag, <body or empty on error>).
    Q_INVOKABLE void fetchJson(const QString &url, const QString &tag);

signals:
    void result(QString text);
    void jsonReady(QString tag, QString json);

private:
    QNetworkAccessManager *m_mgr = nullptr;
};
