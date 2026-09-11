// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// User profile edit dialog: name / avatar char / avatar image.
// Emits saved() and avatarPickRequested(); the host owns persistence feedback
// (profile refresh + toast) and the shared file picker.
Dialog {
    id: profileDialog

    signal saved()
    signal avatarPickRequested()

    property string userAvatarSource: ""
    property bool closeHover: false

    width: 340
    height: 480
    modal: true
    padding: 18
    background: Rectangle {
        // glass mode: dark frosted at 94% opacity for white-text readability
        color: Theme.glassMode ? Qt.rgba(0.14, 0.14, 0.16, 0.94) : Theme.cardFill
        radius: 14
        border.color: Theme.glassBorder
        border.width: 1
    }
    // entrance / exit: fade + scale (not a jarring pop)
    enter: Transition {
        NumberAnimation { property: "scale"; from: 0.94; to: 1.0; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "scale"; to: 0.96; duration: 160; easing.type: Easing.InCubic }
        NumberAnimation { property: "opacity"; to: 0; duration: 160; easing.type: Easing.InCubic }
    }

    function saveProfile() {
        aiService.setUserName(editName.text.trim())
        aiService.setAvatarChar(editAvatar.text.trim())
        profileDialog.saved()
        profileDialog.close()
    }

    header: Item {
        height: 36
        Text {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: "编辑资料"
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
        }
        // ghost close button (no emoji)
        Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "✕"
            color: profileDialog.closeHover ? Theme.text : Theme.textDim
            font.pixelSize: 15
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: profileDialog.closeHover = true
                onExited: profileDialog.closeHover = false
                onClicked: profileDialog.close()
            }
        }
    }

    ColumnLayout {
        width: parent.width
        height: parent.height
        spacing: 12

        // avatar preview — click to change (shows current avatar or char)
        Item {
            Layout.alignment: Qt.AlignHCenter
            width: 96; height: 96
            Rectangle {
                id: userAvatarPreview
                width: 72; height: 72; radius: 36
                anchors.centerIn: parent
                color: Theme.accent
                clip: true
                antialiasing: true
                border.color: Theme.glassBorder
                border.width: 1
                Image {
                    anchors.fill: parent
                    visible: profileDialog.userAvatarSource.length > 0
                    source: profileDialog.userAvatarSource
                    fillMode: Image.PreserveAspectCrop
                    smooth: true; mipmap: true
                }
                Text {
                    anchors.centerIn: parent
                    visible: profileDialog.userAvatarSource.length === 0
                    text: editAvatar.text.length ? editAvatar.text : aiService.avatarChar()
                    color: "white"
                    font.pixelSize: 28
                    font.bold: true
                }
            }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onClicked: profileDialog.avatarPickRequested()
            }
            Text {
                anchors.top: parent.bottom
                anchors.topMargin: 4
                anchors.horizontalCenter: parent.horizontalCenter
                text: "点击更换头像"
                color: Theme.textDim
                font.pixelSize: 11
            }
        }

        Text { text: "名字"; color: Theme.textDim; font.pixelSize: 12 }
        TextField {
            id: editName
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.text
            text: aiService.userName()
            background: Rectangle { color: Theme.inputBg; radius: 8 }
            Keys.onReturnPressed: profileDialog.saveProfile()
        }

        Text { text: "头像文字（单字）"; color: Theme.textDim; font.pixelSize: 12 }
        TextField {
            id: editAvatar
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.text
            text: aiService.avatarChar()
            maximumLength: 1
            background: Rectangle { color: Theme.inputBg; radius: 8 }
            Keys.onReturnPressed: profileDialog.saveProfile()
        }

        AppButton {
            text: "选择头像图片"
            Layout.fillWidth: true
            implicitHeight: 36
            onClicked: profileDialog.avatarPickRequested()
        }

        Text {
            Layout.fillWidth: true
            text: "AI 的名字、人设和头像：打开聊天后点击左上角 AI 头像修改"
            color: Theme.textDim
            font.pixelSize: 11
            wrapMode: Text.Wrap
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            AppButton {
                text: "保存"
                Layout.fillWidth: true
                onClicked: profileDialog.saveProfile()
            }
            AppButton {
                text: "取消"
                variant: "ghost"
                Layout.fillWidth: true
                onClicked: profileDialog.close()
            }
        }
    }
}
