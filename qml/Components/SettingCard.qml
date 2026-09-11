// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Category setting card: leading icon tile + title + description + arrow.
// Telegram/Discord-settings style; hover feedback + press scale.
Rectangle {
    id: root

    property string iconText: ""      // leading glyph (single family, no emoji)
    property string title: ""
    property string description: ""
    property bool hovered: false
    property bool pressed: false
    signal clicked()

    implicitHeight: 76
    // unified card: near-opaque dark (same colour with/without wallpaper),
    // hover slightly brighter. No glassmorphism.
    readonly property color glass: Theme.cardFill
    color: root.hovered ? Theme.cardFillHover : root.glass
    radius: Theme.rXl
    border.color: Theme.glassBorder
    border.width: 1
    Behavior on color { ColorAnimation { duration: Theme.durFast } }

    scale: root.pressed ? 0.99 : root.hovered ? 1.008 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.durMid; easing.type: Easing.OutCubic } }

    // a11y: it is a button (whole card activates)
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: root.title
    Accessible.description: root.description
    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.sp4
        anchors.rightMargin: Theme.sp4
        spacing: Theme.sp4

        Rectangle {
            width: 40; height: 40
            radius: Theme.rLg
            color: Qt.rgba(1,1,1,0.06)
            border.color: Theme.glassBorder
            border.width: 1
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
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Text {
                text: root.description
                color: Theme.text
                opacity: 0.55
                font.pixelSize: Theme.fsSmall
                wrapMode: Text.Wrap
                maximumLineCount: 1
                elide: Text.ElideRight
            }
        }
        Rectangle {
            width: 26; height: 26
            radius: 13
            color: Qt.rgba(1,1,1,0.08)
            Text {
                anchors.centerIn: parent
                text: "›"
                color: Theme.textDim
                font.pixelSize: Theme.fsDefault
            }
        }
    }

    // focus ring (a11y keyboard nav)
    Rectangle {
        anchors.fill: parent
        radius: Theme.rXl
        color: "transparent"
        border.color: Theme.focusRing
        border.width: 1
        visible: root.activeFocus
        opacity: 0.9
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: root.hovered = true
        onExited: { root.hovered = false; root.pressed = false }
        onPressed: root.pressed = true
        onReleased: { root.pressed = false; root.hovered = containsMouse }
        onClicked: root.clicked()
    }
}
