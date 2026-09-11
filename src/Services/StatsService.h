// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

// Usage statistics (ported from the original WinForms report):
// records gacha/clean/mem/proxy events, exposes report data for QML charts.
class StatsService : public QObject
{
    Q_OBJECT
public:
    explicit StatsService(QObject *parent = nullptr);

    Q_INVOKABLE void record(const QString &type, const QString &detail);

    // report data: list of {type, count} for each category
    Q_INVOKABLE QVariantList categoryCounts();          // gacha rarities
    Q_INVOKABLE QVariantList moodCounts();              // mood frequency
    Q_INVOKABLE QVariantList cleanDaily();              // daily freed MB
    Q_INVOKABLE QVariantList proxyCounts();             // clash vs v2rayN
    Q_INVOKABLE QVariantList overviewStats();           // {label, value} list

private:
    QString statsPath() const;
    QString readStats() const;
    void writeStats(const QString &json);
    QStringList lines() const;
};
