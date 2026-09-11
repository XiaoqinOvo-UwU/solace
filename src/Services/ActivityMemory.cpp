// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "ActivityMemory.h"
#include "AppAnalyzer.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QMap>

namespace {
QString dayKey(const QDate &d) { return d.toString("yyyy-MM-dd"); }
} // namespace

void ActivityMemory::load()
{
    m_days = QJsonObject();
    if (m_path.isEmpty()) return;
    QFile f(m_path);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        if (doc.isObject()) m_days = doc.object();
    }
    ensureToday();
}

void ActivityMemory::save()
{
    if (m_path.isEmpty()) return;
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(m_days).toJson(QJsonDocument::Indented));
}

void ActivityMemory::setToday(const QDate &d)
{
    m_today = d;
    ensureToday();
}

void ActivityMemory::ensureToday()
{
    const QString k = dayKey(m_today);
    if (!m_days.contains(k)) {
        QJsonObject day;
        day.insert("apps", QJsonObject());
        day.insert("minutes", 0);
        m_days.insert(k, day);
        save();
    }
}

QJsonObject ActivityMemory::dayObj(const QDate &d) const
{
    return m_days.value(dayKey(d)).toObject();
}

void ActivityMemory::recordMinutes(const QString &appKey, int minutes)
{
    if (minutes <= 0) return;
    ensureToday();
    QJsonObject day = dayObj(m_today);
    QJsonObject apps = day.value("apps").toObject();
    apps.insert(appKey, apps.value(appKey).toInt() + minutes);
    day.insert("apps", apps);
    day.insert("minutes", day.value("minutes").toInt() + minutes);
    m_days.insert(dayKey(m_today), day);
    save();
}

void ActivityMemory::setUptime(const QString &uptimeText)
{
    if (uptimeText.isEmpty()) return;
    ensureToday();
    QJsonObject day = dayObj(m_today);
    day.insert("uptime", uptimeText);
    m_days.insert(dayKey(m_today), day);
    save();
}

int ActivityMemory::dayMinutes(const QDate &d) const
{
    return dayObj(d).value("minutes").toInt();
}

QJsonObject ActivityMemory::dayApps(const QDate &d) const
{
    return dayObj(d).value("apps").toObject();
}

QString ActivityMemory::dayTopApps(const QDate &d, int n, bool withMinutes) const
{
    QJsonObject apps = dayObj(d).value("apps").toObject();
    if (apps.isEmpty()) return QString();
    QMap<int, QString> sorted; // minutes -> label (ascending)
    for (auto it = apps.constBegin(); it != apps.constEnd(); ++it) {
        int mins = it.value().toInt();
        if (mins < 5) continue; // ignore < 5 min blips
        sorted.insert(mins, AppAnalyzer::appLabel(it.key(), it.key()));
    }
    QStringList out;
    auto it = sorted.constEnd();
    for (int i = 0; i < n && it != sorted.constBegin(); ++i) {
        --it;
        out << (withMinutes ? QString("%1（%2 分钟）").arg(it.value()).arg(it.key())
                            : it.value());
    }
    return out.join("、");
}

QString ActivityMemory::todayTopApps(int n, bool withMinutes) const
{
    return dayTopApps(m_today, n, withMinutes);
}

QString ActivityMemory::yesterdayTopApps(int n, bool withMinutes) const
{
    return dayTopApps(m_today.addDays(-1), n, withMinutes);
}

QString ActivityMemory::todayUptime() const
{
    return dayObj(m_today).value("uptime").toString();
}

void ActivityMemory::pruneOlderThan(int days)
{
    QStringList drop;
    for (auto it = m_days.constBegin(); it != m_days.constEnd(); ++it) {
        QDate d = QDate::fromString(it.key(), "yyyy-MM-dd");
        if (d.isValid() && d.daysTo(m_today) > days) drop << it.key();
    }
    if (drop.isEmpty()) return;
    for (const QString &k : drop) m_days.remove(k);
    save();
}

QString ActivityMemory::raw() const
{
    return QString::fromUtf8(QJsonDocument(m_days).toJson(QJsonDocument::Indented));
}
