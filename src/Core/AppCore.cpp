// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "AppCore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryFile>
#include <QTimer>
#ifdef Q_OS_WIN
#include <windows.h>
#include <mmsystem.h>
#endif

AppCore &AppCore::instance()
{
    static AppCore inst;
    return inst;
}

AppCore::AppCore(QObject *parent)
    : QObject(parent)
{
}

void AppCore::navigate(const QString &page)
{
    Q_UNUSED(page)
    // Page switching is handled in QML via StackView; this hook exists for
    // future programmatic navigation / deep links.
}

void AppCore::setStatus(const QString &s)
{
    if (m_status != s) {
        m_status = s;
        emit statusTextChanged();
    }
    // transient statuses revert to "在线" after 4 seconds
    if (s != "在线") {
        m_revertSeq++;
        int seq = m_revertSeq;
        if (!m_revertTimer) {
            m_revertTimer = new QTimer(this);
            m_revertTimer->setSingleShot(true);
            m_revertTimer->setInterval(4000);
            connect(m_revertTimer, &QTimer::timeout, this, [this]() {
                if (m_status != "在线") {
                    m_status = "在线";
                    emit statusTextChanged();
                }
            });
        }
        m_revertTimer->start();
    } else {
        m_revertSeq++; // any "在线" cancels pending revert logic (state is already online)
        if (m_revertTimer) m_revertTimer->stop();
    }
}

void AppCore::showToast(const QString &msg)
{
    m_toastMsg = msg;
    m_toastSeq++;
    emit toastRequested();
}

void AppCore::showIsland(const QString &message, const QStringList &options, const QString &actionId)
{
    m_islandActionId = actionId;
    emit islandRequested(message, options, actionId);
}

void AppCore::selectIslandAction(int index)
{
    emit islandActionChosen(m_islandActionId, index);
}

bool AppCore::isProperLocation()
{
    QString dir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath());
    // recognized locations:
    //  - installed: ...\Program Files\Solace (or \XiaoQinTools for pre-rename installs)
    //  - dev copy:  C:\XiaoQinTools\dist
    if (dir.contains("Program Files", Qt::CaseInsensitive)
        && (dir.endsWith("Solace") || dir.endsWith("XiaoQinTools")))
        return true;
    if (dir.compare("C:\\XiaoQinTools\\dist", Qt::CaseInsensitive) == 0)
        return true;
    return false;
}

void AppCore::playNotify()
{
#ifdef Q_OS_WIN
    // extract the embedded wav to a temp file, then play async (safe with resources)
    static QString cachedPath;
    if (cachedPath.isEmpty()) {
        QFile res(":/notify.wav");
        if (!res.open(QIODevice::ReadOnly)) return;
        QByteArray data = res.readAll();
        res.close();
        cachedPath = QDir::temp().filePath("xiaoqin_notify.wav");
        QFile out(cachedPath);
        if (!out.open(QIODevice::WriteOnly)) return;
        out.write(data);
        out.close();
    }
    PlaySoundW(reinterpret_cast<LPCWSTR>(cachedPath.utf16()), nullptr,
               SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
#endif
}
