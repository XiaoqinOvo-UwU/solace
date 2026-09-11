// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "PluginManager.h"

#include <QDir>
#include <QStandardPaths>

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

QString PluginManager::pluginsDir() const
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString dir = base + "/plugins";
    QDir().mkpath(dir);
    return dir;
}

QStringList PluginManager::listPlugins()
{
    QDir d(pluginsDir());
    QStringList out;
    // look for .qml plugin entry points or subdirectories
    for (const QFileInfo &fi : d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot)) {
        if (fi.isDir() || fi.suffix() == "qml")
            out << fi.fileName();
    }
    return out;
}

bool PluginManager::enablePlugin(const QString &name)
{
    Q_UNUSED(name)
    // reserve: move .disabled -> enabled
    return true;
}

bool PluginManager::disablePlugin(const QString &name)
{
    Q_UNUSED(name)
    // reserve: move -> .disabled
    return true;
}
