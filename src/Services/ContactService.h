// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
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

    // ---- v5.2: pinned contacts (sidebar keeps them on top) ----
    Q_INVOKABLE void setPinned(const QString &id, bool pinned);
    Q_INVOKABLE bool isPinned(const QString &id);

    // ---- v5.2: explicit-id access (a profile edit must never land on another AI) ----
    Q_INVOKABLE QString nameOf(const QString &id);
    Q_INVOKABLE QString personalityOf(const QString &id);
    Q_INVOKABLE void setNameFor(const QString &id, const QString &v);
    Q_INVOKABLE void setPersonalityFor(const QString &id, const QString &v);

    // ---- v5.2: per-AI internal prompt, two independent variants ----
    // kind = "chat" (陪聊真人) | "assistant" (个人助理). Each AI keeps both
    // texts; activePrompt() decides which one is injected into the LLM.
    Q_INVOKABLE QString currentActivePrompt();                 // "chat" | "assistant"
    Q_INVOKABLE void setCurrentActivePrompt(const QString &kind);
    Q_INVOKABLE QString currentPromptText();                   // active variant's text
    Q_INVOKABLE QString currentPromptTextFor(const QString &kind);
    Q_INVOKABLE void setCurrentPromptTextFor(const QString &kind, const QString &text);
    Q_INVOKABLE QString defaultPromptFor(const QString &kind); // preset template

    // id-addressed variants (a profile edit must never touch another AI)
    Q_INVOKABLE QString activePromptOf(const QString &id);
    Q_INVOKABLE QString promptTextFor(const QString &id, const QString &kind);
    Q_INVOKABLE void setPromptTextFor(const QString &id, const QString &kind, const QString &text);
    Q_INVOKABLE void setActivePromptFor(const QString &id, const QString &kind);

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
        bool pinned = false;
        QString promptChat;       // 陪聊真人
        QString promptAssistant;  // 个人助理
        QString activePrompt;     // "chat" | "assistant"
    };
    QList<Contact> m_contacts;
    QString m_currentId;
};
