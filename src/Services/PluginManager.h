// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// Plugin system reserve: scans a plugins directory for future extensibility.
// Currently a directory-based skeleton; plugin ABI will be defined in a later
// release (dynamic libraries exposing a simple interface).
class PluginManager : public QObject
{
    Q_OBJECT
public:
    explicit PluginManager(QObject *parent = nullptr);

    Q_INVOKABLE QStringList listPlugins();           // names of discovered plugins
    Q_INVOKABLE bool enablePlugin(const QString &name);
    Q_INVOKABLE bool disablePlugin(const QString &name);
    QString pluginsDir() const;
};
