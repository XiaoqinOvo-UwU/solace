// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

// Multi-AI contact manager: each contact has its own name, personality,
// avatar image and memory directory under %APPDATA%/XiaoQinTools/contacts/<id>/.
class ContactService : public QObject
{
    Q_OBJECT
public:
    explicit ContactService(QObject *parent = nullptr);

    // singleton accessor for C++ consumers (QML uses the context property)
    static ContactService &instance();

    // list of "id|name|hasAvatar" lines
    Q_INVOKABLE QStringList contactList();

    // add a contact, switch to it, return its id
    Q_INVOKABLE QString addContact(const QString &name, const QString &personality);
    Q_INVOKABLE void removeContact(const QString &id);

    // current contact switching
    Q_INVOKABLE QString currentId();
    Q_INVOKABLE void setCurrent(const QString &id);
    Q_INVOKABLE QString currentName();
    Q_INVOKABLE QString currentPersonality();
    Q_INVOKABLE void setCurrentName(const QString &v);
    Q_INVOKABLE void setCurrentPersonality(const QString &v);

    // ---- structured character card (character-card-v2 style) ----
    // scenario = 场景/关系设定, examples = 示例对话(教学语气),
    // firstMessages = 多条角色开场白(随机挑选)
    Q_INVOKABLE QString currentScenario();
    Q_INVOKABLE QString currentExamples();
    Q_INVOKABLE QStringList currentFirstMessages();
    Q_INVOKABLE void setCurrentScenario(const QString &v);
    Q_INVOKABLE void setCurrentExamples(const QString &v);
    Q_INVOKABLE void setCurrentFirstMessages(const QStringList &v);

    // avatar: copy a local image into the contact dir, return stored path
    Q_INVOKABLE QString setCurrentAvatar(const QString &srcPath);
    Q_INVOKABLE QString currentAvatarPath();

    // helpers for AiService (per-contact dirs)
    QString contactDir(const QString &id) const;
    QString contactMemoryPath(const QString &id) const;
    Q_INVOKABLE QString contactAvatarPath(const QString &id) const;
    // display-ready file:// url with a cache-buster (#mtime) so QML Image
    // reloads when the avatar file changes
    Q_INVOKABLE QString contactAvatarUrl(const QString &id) const;
    Q_INVOKABLE QString currentAvatarUrl() const;

    // ensure a default contact exists (first run)
    void ensureDefault();

signals:
    void contactsChanged();

private:
    void load();
    void save();
    QStringList ids() const;

    struct Contact {
        QString id;
        QString name;
        QString personality;
        QString scenario;
        QString examples;
        QStringList firstMessages;
    };
    QList<Contact> m_contacts;
    QString m_currentId;
};
