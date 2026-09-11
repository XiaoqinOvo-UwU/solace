// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Profile header: avatar + name + online status. Used at the top of the
// AI profile view. Clicking the avatar opens the picker.
Rectangle {
    id: root

    property string avatarSource: ""
    property string name: ""
    property string status: "在线"
    property bool statusOnline: true
    signal avatarClicked()

    color: Theme.cardFill
    radius: Theme.rXl
    border.color: Theme.glassBorder
    border.width: 1

    implicitHeight: 96

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.sp5
        anchors.rightMargin: Theme.sp5
        spacing: Theme.sp4

        Avatar {
            id: avatar
            size: 56
            source: root.avatarSource
            charText: root.name
            pressable: true
            onClicked: root.avatarClicked()
        }
        Column {
            Layout.fillWidth: true
            spacing: Theme.sp1
            Text {
                text: root.name
                color: Theme.text
                font.pixelSize: Theme.fsTitle
                font.bold: true
                elide: Text.ElideRight
            }
            Row {
                spacing: Theme.sp2
                Rectangle {
                    width: 8; height: 8; radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.statusOnline ? Theme.ok : Theme.textDim
                }
                Text {
                    text: root.status
                    color: Theme.textDim
                    font.pixelSize: Theme.fsSmall
                }
            }
        }
        Text {
            text: "更换头像"
            color: Theme.textDim
            font.pixelSize: Theme.fsCaption
        }
    }
}
