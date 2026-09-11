// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "SyncService.h"
#include "ConfigService.h"

#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

SyncService::SyncService(QObject *parent)
    : QObject(parent)
{
}

QString SyncService::defaultExportPath()
{
    return ConfigService::instance().configDir() + "/配置导出.json";
}

bool SyncService::exportConfig(const QString &destPath)
{
    QJsonObject o;
    o.insert("base_url", ConfigService::instance().baseUrl());
    o.insert("model", ConfigService::instance().model());
    // secrets are NEVER exported — the field is written empty
    o.insert("api_key", QString());
    o.insert("clash_path", ConfigService::instance().clashPath());
    o.insert("v2ray_path", ConfigService::instance().v2rayPath());
    o.insert("exported_at", QDateTime::currentDateTime().toString(Qt::ISODate));

    QFile f(destPath);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(QJsonDocument(o).toJson());
    f.close();
    return true;
}

bool SyncService::importConfig(const QString &srcPath)
{
    QFile f(srcPath);
    if (!f.open(QIODevice::ReadOnly)) return false;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (!doc.isObject()) return false;
    QJsonObject o = doc.object();

    if (o.contains("base_url")) ConfigService::instance().setBaseUrl(o.value("base_url").toString());
    if (o.contains("model")) ConfigService::instance().setModel(o.value("model").toString());
    // never import an EMPTY api_key: exports deliberately blank this field,
    // and importing our own backup must not wipe the locally saved key
    if (o.contains("api_key") && !o.value("api_key").toString().trimmed().isEmpty())
        ConfigService::instance().setApiKey(o.value("api_key").toString());
    return true;
}
