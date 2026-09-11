// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls

// Generic frosted-glass action card used across pages.
Rectangle {
    id: card
    property string title: ""
    property string desc: ""
    property color textColor: Theme.text
    property bool hovered: false
    property bool pressed: false
    signal clicked()

    // material follows Theme tokens (solid dark / wallpaper translucent /
    // glass) so the Wallpaper Glass mode applies automatically.
    implicitWidth: 180
    implicitHeight: 92
    radius: 12
    color: pressed ? Theme.cardFillPress
         : hovered ? Theme.cardFillHover
         : Theme.cardFill
    border.color: Theme.glassBorder
    border.width: 1

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onEntered: card.hovered = true
        onExited: {
            card.hovered = false
            card.pressed = false
        }
        onPressed: card.pressed = true
        onReleased: {
            card.pressed = false
            card.hovered = mouse.containsMouse
        }
        onClicked: card.clicked()
    }

    Column {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 6
        Text {
            text: card.title
            color: card.textColor
            font.pixelSize: 15
            font.bold: true
        }
        Text {
            text: card.desc
            color: Theme.textDim
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }
}
