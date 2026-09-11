// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// AppCore is the single bridge between QML and C++ business logic.
// Services are created and injected in main.cpp; AppCore exposes a clean
// QML-facing API surface and keeps cross-service state (status, toasts...).
class AppCore : public QObject
{
    Q_OBJECT
public:
    static AppCore &instance();

    // navigation
    Q_INVOKABLE void navigate(const QString &page);

    // status line for the right side / status area
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    QString statusText() const { return m_status; }
    Q_INVOKABLE void setStatus(const QString &s);   // transient: reverts to "在线" after a few seconds

    // island toast
    Q_INVOKABLE void showToast(const QString &msg);
    Q_PROPERTY(QString toastMessage READ toastMessage NOTIFY toastRequested)
    Q_PROPERTY(int toastSeq READ toastSeq NOTIFY toastRequested)
    QString toastMessage() const { return m_toastMsg; }
    int toastSeq() const { return m_toastSeq; }

    // ---- single dynamic-island (message + optional action buttons) ----
    // Routes every island request to the ONE island instance in Main.qml, so
    // the whole app can never show more than one island at a time.
    Q_INVOKABLE void showIsland(const QString &message, const QStringList &options = {},
                                const QString &actionId = QString());
    Q_INVOKABLE void selectIslandAction(int index);   // called by the island UI

    // returns true if the exe runs from a recognized install location
    Q_INVOKABLE bool isProperLocation();

    // play the notification sound (embedded resource) — non-blocking
    Q_INVOKABLE void playNotify();

signals:
    void statusTextChanged();
    void toastRequested();
    void islandRequested(QString message, QStringList options, QString actionId);
    void islandActionChosen(QString actionId, int index);

private:
    explicit AppCore(QObject *parent = nullptr);
    QString m_status = "在线";
    QString m_toastMsg;
    int m_toastSeq = 0;
    QString m_islandActionId;
    class QTimer *m_revertTimer = nullptr;
    int m_revertSeq = 0;      // guard so only the latest status reverts
};
