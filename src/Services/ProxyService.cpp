// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "ProxyService.h"
#include "ConfigService.h"

#include <QProcess>
#include <QTcpSocket>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QClipboard>
#include <QGuiApplication>
#include <QtConcurrent>
#include <QFutureWatcher>

ProxyService::ProxyService(QObject *parent)
    : QObject(parent)
{
}

bool ProxyService::processRunning(const QString &name)
{
    // Query all processes and compare base name (case-insensitive).
    // `tasklist` approach keeps us dependency-light on Windows.
    QProcess p;
    p.start("tasklist", QStringList() << "/FI" << ("IMAGENAME eq " + name + ".exe"), QIODevice::ReadOnly);
    p.waitForFinished(1500);
    QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    return out.contains(name + ".exe", Qt::CaseInsensitive);
}

bool ProxyService::portOpen(int port)
{
    QTcpSocket sock;
    sock.connectToHost("127.0.0.1", port);
    return sock.waitForConnected(400);
}

// static port probe usable from worker threads
static bool portOpenStatic(int port)
{
    QTcpSocket sock;
    sock.connectToHost("127.0.0.1", port);
    return sock.waitForConnected(400);
}

bool ProxyService::isClashRunning()
{
    return processRunning("clash-verge") || processRunning("verge-mihomo");
}

bool ProxyService::isV2rayRunning()
{
    return processRunning("v2rayN") || processRunning("xray");
}

bool ProxyService::isClashPortOpen() { return portOpen(7897); }
bool ProxyService::isV2rayPortOpen() { return portOpen(10808); }

QString ProxyService::clashExe()
{
    QString c = ConfigService::instance().clashPath();
    if (!c.isEmpty() && QFileInfo::exists(c)) return c;
    return ConfigService::instance().detectClashExe();
}

QString ProxyService::v2rayExe()
{
    QString c = ConfigService::instance().v2rayPath();
    if (!c.isEmpty() && QFileInfo::exists(c)) return c;
    return ConfigService::instance().detectV2rayExe();
}

bool ProxyService::launchClash()
{
    QString exe = clashExe();
    if (exe.isEmpty()) return false;
    return QProcess::startDetached(exe, QStringList());
}

bool ProxyService::launchV2ray()
{
    QString exe = v2rayExe();
    if (exe.isEmpty()) return false;
    return QProcess::startDetached(exe, QStringList());
}

bool ProxyService::launchAny()
{
    if (!clashExe().isEmpty()) return launchClash();
    if (!v2rayExe().isEmpty()) return launchV2ray();
    return false;
}

bool ProxyService::hasClash() { return !clashExe().isEmpty(); }
bool ProxyService::hasV2ray() { return !v2rayExe().isEmpty(); }

void ProxyService::copyToClipboard(const QString &text)
{
    if (QGuiApplication::clipboard())
        QGuiApplication::clipboard()->setText(text);
}

QString ProxyService::quickStatus()
{
    bool clash = isClashPortOpen();
    bool v2ray = isV2rayPortOpen();
    if (clash) return "Clash 已连接 ●";
    if (v2ray) return "v2rayN 已连接 ●";
    return "未运行 ✕";
}

void ProxyService::statusAsync()
{
    // Run port probing on a worker thread so the UI never blocks.
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        emit statusReady(watcher->result());
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([]() {
        // static helpers, no `this` capture -> thread-safe
        bool clash = portOpenStatic(7897);
        bool v2ray = portOpenStatic(10808);
        if (clash) return QString("Clash 已连接 ●");
        if (v2ray) return QString("v2rayN 已连接 ●");
        return QString("未运行 ✕");
    });
    watcher->setFuture(future);
}
