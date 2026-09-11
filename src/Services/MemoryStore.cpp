// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "MemoryStore.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QUuid>

// ---- CoreMemory (de)serialization ----
QJsonObject CoreMemory::toJson() const
{
    QJsonObject o;
    o.insert("id", id);
    o.insert("text", text);
    if (!category.isEmpty())     o.insert("category", category);
    if (!emotion.isEmpty())      o.insert("emotion", emotion);
    if (!relationship.isEmpty()) o.insert("relationship", relationship);
    o.insert("importance", importance);
    o.insert("confidence", confidence);
    if (!source.isEmpty())       o.insert("source", source);
    o.insert("created", created.isValid() ? created.toString(Qt::ISODate)
                                          : QDateTime::currentDateTime().toString(Qt::ISODate));
    return o;
}

CoreMemory CoreMemory::fromJson(const QJsonObject &o)
{
    CoreMemory r;
    r.id           = o.value("id").toString();
    r.text         = o.value("text").toString();
    r.category     = o.value("category").toString();
    r.emotion      = o.value("emotion").toString();
    r.relationship = o.value("relationship").toString();
    r.importance   = o.value("importance").toDouble(0.5);
    r.confidence   = o.value("confidence").toDouble(0.5);
    r.source       = o.value("source").toString();
    r.created      = QDateTime::fromString(o.value("created").toString(), Qt::ISODate);
    if (!r.created.isValid()) r.created = QDateTime::currentDateTime();
    return r;
}

// ---- MemoryStore ----
MemoryStore::MemoryStore(QString memoryFilePath)
    : m_path(std::move(memoryFilePath))
{
}

void MemoryStore::load()
{
    m_records.clear();
    m_dirty = false;

    QFile f(m_path);
    QJsonObject mem;
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (d.isObject()) mem = d.object();
    }

    const QJsonValue core = mem.value("memoryCore");
    if (core.isArray()) {
        for (const QJsonValue &v : core.toArray())
            if (v.isObject()) m_records << CoreMemory::fromJson(v.toObject());
    } else {
        // first run under v5.0: migrate legacy notes/events
        m_records = migrateFromLegacy(mem);
        m_dirty = !m_records.isEmpty();   // persist the migrated core on next save()
    }
}

void MemoryStore::save() const
{
    QFile f(m_path);
    QJsonObject mem;
    if (f.open(QIODevice::ReadOnly)) {
        const QJsonDocument d = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (d.isObject()) mem = d.object();   // preserve AiService-owned keys
    }

    QJsonArray arr;
    for (const CoreMemory &r : m_records) arr.append(r.toJson());
    mem.insert("memoryCore", arr);

    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(mem).toJson());
        f.close();
    }
}

void MemoryStore::add(const CoreMemory &r)
{
    CoreMemory rec = r;
    if (rec.id.isEmpty())
        rec.id = "mem_" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    if (!rec.created.isValid()) rec.created = QDateTime::currentDateTime();
    m_records << rec;
    m_dirty = true;
}

void MemoryStore::clear()
{
    m_records.clear();
    m_dirty = true;
}

QVector<CoreMemory> MemoryStore::migrateFromLegacy(const QJsonObject &mem)
{
    QVector<CoreMemory> out;

    // notes[] : "text（yyyy-MM-dd HH:mm）" strings the user asked to remember.
    // Treated like user facts (high trust / high importance).
    const QJsonArray notes = mem.value("notes").toArray();
    for (const QJsonValue &v : notes) {
        const QString s = v.toString().trimmed();
        if (s.isEmpty()) continue;
        CoreMemory r;
        r.text         = s;
        r.source       = "note";
        r.category     = "user";
        r.importance   = 1.0;
        r.confidence   = 1.0;
        r.created      = QDateTime::currentDateTime();
        out << r;
    }

    // events[] : { date, time, type, summary } shared experiences.
    const QJsonArray events = mem.value("events").toArray();
    for (const QJsonValue &v : events) {
        const QJsonObject ev = v.toObject();
        const QString summary = ev.value("summary").toString().trimmed();
        if (summary.isEmpty()) continue;
        CoreMemory r;
        r.text         = summary;
        r.source       = "event";
        r.category     = ev.value("type").toString();  // e.g. "mood" / "milestone"
        r.importance   = 0.75;
        r.confidence   = 0.85;
        const QString date = ev.value("date").toString();
        const QString time = ev.value("time").toString();
        r.created = QDateTime::fromString(date + " " + time, "yyyy-MM-dd HH:mm");
        if (!r.created.isValid()) r.created = QDateTime::currentDateTime();
        out << r;
    }

    return out;
}
