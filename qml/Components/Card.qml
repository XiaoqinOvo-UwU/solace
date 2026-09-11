// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Reusable surface card: raised surface layer + hairline border.
// Variants: static (display) or clickable (hover/press feedback).
Rectangle {
    id: root

    property string title: ""       // optional card title
    property string subtitle: ""    // optional support text
    property bool clickable: false
    property bool hovered: false
    property bool pressed: false
    signal clicked()

    // material: one raised layer + hairline (M3/HIG tone-layering, no shadows).
    // With the wallpaper on, the card stays the SAME dark colour at ~93%
    // opacity — no glassmorphism, no hue shift; the wallpaper only tints the
    // background layer behind it.
    readonly property color glass: Theme.cardFill
    color: root.pressed ? Theme.cardFillPress
         : root.hovered ? Theme.cardFillHover
         : root.glass
    border.color: Theme.glassBorder
    radius: Theme.rXl
    border.width: 1
    clip: true
    Behavior on color { ColorAnimation { duration: Theme.durFast } }

    // hover lift (kept): grids leave spacing so the enlarged card never clips
    scale: root.pressed ? 0.99 : root.hovered ? 1.008 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.durMid; easing.type: Easing.OutCubic } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.sp4
        spacing: Theme.sp1
        visible: root.title.length > 0 || root.subtitle.length > 0
        Text {
            Layout.fillWidth: true
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fsDefault
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            visible: root.title.length > 0
        }
        Text {
            Layout.fillWidth: true
            text: root.subtitle
            color: Theme.text
            opacity: 0.55
            font.pixelSize: Theme.fsSmall
            wrapMode: Text.Wrap
            visible: root.subtitle.length > 0
        }
    }

    MouseArea {
        anchors.fill: parent
        visible: root.clickable
        hoverEnabled: true
        onEntered: root.hovered = true
        onExited: { root.hovered = false; root.pressed = false }
        onPressed: root.pressed = true
        onReleased: { root.pressed = false; root.hovered = containsMouse }
        onClicked: root.clicked()
    }
}
