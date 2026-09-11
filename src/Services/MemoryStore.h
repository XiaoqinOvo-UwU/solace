// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QString>
#include <QVector>
#include <QDateTime>
#include <QJsonObject>

// =====================================================================
// MemoryStore — v5.0 Memory Core persistence.
//
// Owns the `memoryCore[]` section of a contact's memory.json. Each record
// is a "personal model" entry: the fact PLUS why it matters
// (category / emotion / relationship / importance / confidence).
//
// Additive & backward compatible: every other key in memory.json
// (notes / events / chatLog / ... owned by AiService) is preserved
// untouched. On first load, existing notes[]/events[] are migrated into
// memoryCore[] with sensible defaults; the legacy keys are NOT deleted.
//
// On-disk shape (inside memory.json):
//   "memoryCore": [
//     { "id","text","category","emotion","relationship",
//       "importance","confidence","source","created" }, ... ]
// =====================================================================
struct CoreMemory
{
    QString   id;
    QString   text;
    QString   category;      // project / habit / health / social / interest / emotion / ...
    QString   emotion;       // user emotion at the time: stressed / happy / lonely / ...
    QString   relationship;  // relational need: 需要鼓励 / 想倾诉 / 陪伴 ...
    double    importance = 0.5;   // 0..1
    double    confidence = 0.5;   // 0..1
    QString   source;        // "note" | "event" | "ai"
    QDateTime created;

    QJsonObject toJson() const;
    static CoreMemory fromJson(const QJsonObject &o);
};

class MemoryStore
{
public:
    // `memoryFilePath` = a contact's memory.json (e.g. contactDir + "/memory.json")
    explicit MemoryStore(QString memoryFilePath);

    void load();                 // read file; migrate legacy notes/events if memoryCore missing
    void save() const;           // write memoryCore[] back, preserving all other keys

    bool dirty() const { return m_dirty; }
    const QVector<CoreMemory> &records() const { return m_records; }

    void add(const CoreMemory &r);   // append + mark dirty (call save() to persist)
    void clear();                    // wipe memoryCore

    // build records from legacy notes[]/events[] when memoryCore is absent
    static QVector<CoreMemory> migrateFromLegacy(const QJsonObject &mem);

private:
    QString m_path;
    QVector<CoreMemory> m_records;
    bool m_dirty = false;
};
