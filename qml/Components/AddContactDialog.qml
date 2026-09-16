// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Add an AI contact: name + one-line persona. Creation goes through
// ContactService, which persists and emits contactsChanged, so the sidebar
// refreshes itself; the host only needs the new id for feedback.
DialogContainer {
    id: addDialog

    signal created(string contactId)

    dialogTitle: "添加 AI 联系人"
    dialogSubtitle: "名字必填；人设、头像、内部提示词创建后都能改"
    dialogWidth: 420
    dialogHeight: 340

    onClosed: {
        editName.text = ""
        editPersona.text = ""
    }

    function createContact() {
        const name = editName.text.trim()
        if (name.length === 0) {
            editName.forceActiveFocus()
            return
        }
        const id = contactService.addContact(name, editPersona.text.trim())
        addDialog.created(id)
        addDialog.close()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.sp5
        spacing: Theme.sp3

        Text { text: "名字"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
        ThemedTextField {
            id: editName
            placeholderText: "给这个 AI 起个名字"
            Keys.onReturnPressed: addDialog.createContact()
        }

        Text { text: "人设（一句话也行）"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
        ThemedTextField {
            id: editPersona
            placeholderText: "比如：温柔、嘴硬心软、爱吐槽"
            Keys.onReturnPressed: addDialog.createContact()
        }

        Text {
            Layout.fillWidth: true
            text: "每个 AI 的称呼、头像、人设和两套内部提示词（陪聊真人 / 个人助理）都是独立的。"
            color: Theme.textDim
            font.pixelSize: Theme.fsCaption
            wrapMode: Text.Wrap
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.sp3
            AppButton {
                text: "创建"
                Layout.fillWidth: true
                onClicked: addDialog.createContact()
            }
            AppButton {
                text: "取消"
                variant: "ghost"
                Layout.fillWidth: true
                onClicked: addDialog.close()
            }
        }
    }
}
