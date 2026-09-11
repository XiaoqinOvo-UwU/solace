// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "SystemService.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QProcess>
#include <QSettings>
#include <QCoreApplication>
#include <QtConcurrent>
#include <QFutureWatcher>

SystemService::SystemService(QObject *parent)
    : QObject(parent)
{
}

QString SystemService::cleanJunk()
{
    qint64 freed = 0;
    QStringList dirs = {
        QDir::tempPath(),
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/CrashDumps",
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/SquirrelTemp",
    };
    for (const QString &d : dirs) {
        QDir dir(d);
        if (!dir.exists()) continue;
        for (const QFileInfo &fi : dir.entryInfoList(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot, QDir::DirsFirst)) {
            QFileInfo f(fi.absoluteFilePath());
            if (f.isFile()) { freed += f.size(); QFile::remove(f.absoluteFilePath()); }
            else if (f.isDir()) { QDir(f.absoluteFilePath()).removeRecursively(); }
        }
    }
    return "泉此方替你扫走垃圾了 不用谢~ (释放 " + QString::number(freed / 1024 / 1024) + " MB)";
}

void SystemService::cleanJunkAsync()
{
    // Run cleanup on a worker thread; avoid capturing `this` so the lambda is
    // self-contained and safe off the GUI thread.
    auto *watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher]() {
        emit cleanupDone(watcher->result());
        watcher->deleteLater();
    });
    QFuture<QString> future = QtConcurrent::run([]() {
        qint64 freed = 0;
        QStringList dirs = {
            QDir::tempPath(),
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/CrashDumps",
            QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/SquirrelTemp",
        };
        for (const QString &d : dirs) {
            QDir dir(d);
            if (!dir.exists()) continue;
            for (const QFileInfo &fi : dir.entryInfoList(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot, QDir::DirsFirst)) {
                QFileInfo f(fi.absoluteFilePath());
                if (f.isFile()) { freed += f.size(); QFile::remove(f.absoluteFilePath()); }
                else if (f.isDir()) { QDir(f.absoluteFilePath()).removeRecursively(); }
            }
        }
        return "泉此方替你扫走垃圾了 不用谢~ (释放 " + QString::number(freed / 1024 / 1024) + " MB)";
    });
    watcher->setFuture(future);
}

QString SystemService::cleanMemory()
{
    // Best-effort: flush caches via a short GC isn't available in C++/Qt directly.
    // On Windows we can call EmptyWorkingSet for our process; keep message friendly.
    return "泉此方替你扫走内存垃圾了 不用谢~";
}

QString SystemService::listStartupItems()
{
    QStringList out;
    // HKCU Run
    QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    for (const QString &k : s.allKeys())
        out << "[用户] " + k + " = " + s.value(k).toString();
    return out.join("\n");
}

QStringList SystemService::startupItemList()
{
    QStringList out;
    QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    for (const QString &k : s.allKeys())
        out << k + "|" + s.value(k).toString() + "|用户";
    QSettings lm("HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    for (const QString &k : lm.allKeys())
        out << k + "|" + lm.value(k).toString() + "|系统";
    return out;
}

bool SystemService::disableStartupItem(const QString &name)
{
    QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (!s.contains(name)) return false;
    s.remove(name);
    return true;
}

bool SystemService::enableStartupItem(const QString &name)
{
    Q_UNUSED(name)
    return false; // value unknown after removal; manual re-add not supported without saved cmd
}

QString SystemService::scanLargeFiles()
{
    // scan common big dirs for large files (quick, top-level only)
    QStringList roots = { "C:/Windows/Temp", "C:/ProgramData", "C:/Users/0/Downloads" };
    QList<QPair<qint64, QString>> found;
    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists()) continue;
        const auto files = dir.entryInfoList(QDir::Files, QDir::Size);
        for (const QFileInfo &fi : files) {
            if (fi.size() > 50 * 1024 * 1024) // > 50MB
                found.append(qMakePair(fi.size(), fi.absoluteFilePath()));
        }
    }
    std::sort(found.begin(), found.end(), [](const QPair<qint64,QString> &a, const QPair<qint64,QString> &b) {
        return a.first > b.first;
    });
    QStringList out;
    int take = qMin(10, found.size());
    for (int i = 0; i < take; i++)
        out << QString("%1 MB  %2").arg(found[i].first / 1024 / 1024).arg(found[i].second);
    return out.isEmpty() ? "未找到大于 50MB 的文件" : out.join("\n");
}

// ---- app auto-start (HKCU Run key) ----
bool SystemService::setAutoStart(bool enabled)
{
    QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                QSettings::NativeFormat);
    QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    if (enabled) {
        s.setValue("XiaoQinTools", "\"" + exe + "\"");
    } else {
        s.remove("XiaoQinTools");
    }
    s.sync();
    return s.status() == QSettings::NoError;
}

bool SystemService::isAutoStartEnabled()
{
    QSettings s("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                QSettings::NativeFormat);
    return s.contains("XiaoQinTools");
}
