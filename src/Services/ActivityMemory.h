// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QJsonObject>
#include <QDate>

// =====================================================================
// ActivityMemory — short-term computer-usage memory, kept SEPARATE from
// the long-term personality memory (memory.json notes[]).
//
// - stores daily app usage: { date -> { apps: {key: minutes}, minutes, uptime } }
// - defaults to keeping ~14-30 days, then prunes
// - NEVER enters user personality memory (memory.json stays clean)
//
// File: <configDir>/activity_memory.json (memory.json untouched).
// =====================================================================

class ActivityMemory
{
public:
    void setPath(const QString &p) { m_path = p; load(); }
    void setToday(const QDate &d);                      // for tests / day rollover

    void recordMinutes(const QString &appKey, int minutes); // accumulate into today
    void setUptime(const QString &uptimeText);              // store today's boot uptime

    int    dayMinutes(const QDate &d) const;
    QJsonObject dayApps(const QDate &d) const;              // { appKey: minutes } for a day
    QString dayTopApps(const QDate &d, int n, bool withMinutes) const;
    QString todayTopApps(int n, bool withMinutes) const;
    QString yesterdayTopApps(int n, bool withMinutes) const;
    QString todayUptime() const;

    void pruneOlderThan(int days);                      // keep the last N days

    QString raw() const;                                // full JSON text (debug/tests)
    void reload() { load(); }

private:
    QJsonObject dayObj(const QDate &d) const;
    void ensureToday();
    void load();
    void save();

    QString m_path;
    QJsonObject m_days;                                 // "yyyy-MM-dd" -> day object
    QDate m_today = QDate::currentDate();
};
