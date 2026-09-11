// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "StatsService.h"
#include "MoodService.h"
#include "ConfigService.h"

#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QDate>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

StatsService::StatsService(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(ConfigService::instance().configDir());
}

QString StatsService::statsPath() const
{
    return ConfigService::instance().configDir() + "/stats.txt";
}

void StatsService::record(const QString &type, const QString &detail)
{
    QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm") + "|" + type + "|" + detail + "\r\n";
    QFile f(statsPath());
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        f.write(line.toUtf8());
        f.close();
    }
}

QStringList StatsService::lines() const
{
    QFile f(statsPath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    QStringList out = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    f.close();
    return out;
}

QVariantList StatsService::categoryCounts()
{
    int ssr = 0, sr = 0, r = 0, n = 0;
    for (const QString &l : lines()) {
        if (!l.contains("|gacha|")) continue;
        if (l.contains("SSR")) ssr++;
        else if (l.contains("SR")) sr++;
        else if (l.contains("R ")) r++;
        else n++;
    }
    QVariantList out;
    out << QVariantMap{{"label", "SSR"}, {"value", ssr}};
    out << QVariantMap{{"label", "SR"}, {"value", sr}};
    out << QVariantMap{{"label", "R"}, {"value", r}};
    out << QVariantMap{{"label", "N"}, {"value", n}};
    return out;
}

QVariantList StatsService::moodCounts()
{
    QMap<QString, int> map;
    for (const QString &l : lines()) {
        if (!l.contains("|mood|")) continue;
        int idx = l.indexOf("|mood|") + 6;
        QString m = l.mid(idx).trimmed();
        map[m]++;
    }
    QVariantList out;
    QList<QPair<int, QString>> sorted;
    for (auto it = map.begin(); it != map.end(); ++it)
        sorted.append({it.value(), it.key()});
    std::sort(sorted.begin(), sorted.end(), [](const QPair<int,QString> &a, const QPair<int,QString> &b) {
        return a.first > b.first;
    });
    int take = qMin(8, sorted.size());
    for (int i = 0; i < take; i++)
        out << QVariantMap{{"label", sorted[i].second}, {"value", sorted[i].first}};
    return out;
}

QVariantList StatsService::cleanDaily()
{
    QMap<QString, int> map;
    for (const QString &l : lines()) {
        if (!l.contains("|clean|")) continue;
        QString date = l.left(10);
        QStringList parts = l.split('|');
        if (parts.size() >= 3) {
            bool ok = false;
            int mb = parts[2].trimmed().toInt(&ok);
            if (ok) map[date] += mb;
        }
    }
    QVariantList out;
    // last 10 days with data
    QList<QPair<QString, int>> items;
    for (auto it = map.begin(); it != map.end(); ++it)
        items.append({it.key(), it.value()});
    std::sort(items.begin(), items.end(), [](const QPair<QString,int> &a, const QPair<QString,int> &b) {
        return a.first < b.first;
    });
    int start = qMax(0, items.size() - 10);
    for (int i = start; i < items.size(); i++)
        out << QVariantMap{{"label", items[i].first.mid(5)}, {"value", items[i].second}};
    return out;
}

QVariantList StatsService::proxyCounts()
{
    int clash = 0, v2ray = 0;
    for (const QString &l : lines()) {
        if (!l.contains("|proxy|")) continue;
        if (l.contains("Clash")) clash++;
        else v2ray++;
    }
    QVariantList out;
    out << QVariantMap{{"label", "Clash"}, {"value", clash}};
    out << QVariantMap{{"label", "v2rayN"}, {"value", v2ray}};
    return out;
}

QVariantList StatsService::overviewStats()
{
    int gacha = 0, clean = 0, mem = 0, proxy = 0, mood = 0;
    int totalMb = 0;
    for (const QString &l : lines()) {
        if (l.contains("|gacha|")) gacha++;
        else if (l.contains("|clean|")) {
            clean++;
            QStringList p = l.split('|');
            if (p.size() >= 3) { bool ok = false; int mb = p[2].trimmed().toInt(&ok); if (ok) totalMb += mb; }
        }
        else if (l.contains("|mem|")) mem++;
        else if (l.contains("|proxy|")) proxy++;
        else if (l.contains("|mood|")) mood++;
    }
    QVariantList out;
    out << QVariantMap{{"label", "抽卡次数"}, {"value", gacha}};
    out << QVariantMap{{"label", "清理垃圾"}, {"value", clean}};
    out << QVariantMap{{"label", "释放空间"}, {"value", totalMb + " MB"}};
    out << QVariantMap{{"label", "清理内存"}, {"value", mem}};
    out << QVariantMap{{"label", "打开梯子"}, {"value", proxy}};
    out << QVariantMap{{"label", "心情记录"}, {"value", mood}};
    return out;
}
