// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Settings row item: leading icon + title + optional subtitle + trailing
// control slot (arrow/switch/value). Visible card outline + hover feedback.
Rectangle {
    id: root

    property string iconText: ""     // leading glyph (single family)
    property string title: ""
    property string subtitle: ""
    property string value: ""        // trailing text value
    property bool clickable: false
    property bool hovered: false
    signal clicked()

    // visible resting state (not transparent): glass fill + hairline border
    color: root.hovered ? Theme.hoverBgStrong : Theme.glass
    radius: Theme.rMd
    border.color: Theme.glassBorder
    border.width: 1
    Behavior on color { ColorAnimation { duration: Theme.durFast } }

    height: subtitle.length > 0 ? 64 : 52

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.sp3
        anchors.rightMargin: Theme.sp3
        spacing: Theme.sp3

        Rectangle {
            width: 34; height: 34
            radius: Theme.rMd
            color: Qt.rgba(1,1,1,0.06)
            visible: root.iconText.length > 0
            Text {
                anchors.centerIn: parent
                text: root.iconText
                color: Theme.textMuted
                font.pixelSize: Theme.fsDefault
            }
        }
        Column {
            Layout.fillWidth: true
            spacing: 2
            Text {
                text: root.title
                color: Theme.text
                font.pixelSize: Theme.fsDefault
                font.bold: root.clickable
                elide: Text.ElideRight
            }
            Text {
                text: root.subtitle
                color: Theme.textDim
                font.pixelSize: Theme.fsCaption
                elide: Text.ElideRight
                visible: root.subtitle.length > 0
            }
        }
        Text {
            text: root.value
            color: Theme.textDim
            font.pixelSize: Theme.fsSmall
            visible: root.value.length > 0
        }
        Rectangle {
            width: 24; height: 24
            radius: 12
            color: Qt.rgba(1,1,1,0.08)
            visible: root.clickable
            Text {
                anchors.centerIn: parent
                text: "›"
                color: Theme.textDim
                font.pixelSize: Theme.fsSmall
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        visible: root.clickable
        hoverEnabled: true
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: root.clicked()
    }
}
