// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls

// App button with explicit variants (design-system):
//   variant "primary"   — solid accent, main action
//   variant "secondary" — glass fill, common action
//   variant "ghost"     — transparent, low emphasis
// States: default / hover / pressed / disabled / focus (a11y ring)
Button {
    id: root

    property string variant: "secondary"   // primary | secondary | ghost
    property int btnRadius: Theme.rMd
    property int btnHeight: 38

    implicitHeight: btnHeight
    font.pixelSize: Theme.fsDefault
    font.family: "Microsoft YaHei UI"
    focusPolicy: Qt.StrongFocus

    // legacy overrides (kept for existing call sites; ignored when unset)
    property color glassColor: Qt.rgba(0,0,0,0)      // sentinel: unset
    property color glassHover: Qt.rgba(0,0,0,0)
    property color glassPress: Qt.rgba(0,0,0,0)
    property color borderColor: Qt.rgba(0,0,0,0)

    property color vFill: variant === "primary" ? Theme.accent
                        : variant === "ghost" ? "transparent"
                        : Theme.wallpaperActive ? Theme.btnFill
                        : (glassColor.a > 0 ? glassColor : Qt.rgba(1,1,1,0.08))
    // hover/press/disabled overlays flip to dark tints on white glass, where
    // white-on-white overlays would be invisible
    property color vFillHover: variant === "primary" ? Theme.accentHover
                             : variant === "ghost" ? (Theme.glassMode ? Qt.rgba(0,0,0,0.06) : Qt.rgba(1,1,1,0.05))
                             : Theme.wallpaperActive ? Theme.btnFillHover
                             : (glassHover.a > 0 ? glassHover : Qt.rgba(1,1,1,0.14))
    property color vFillPress: variant === "primary" ? Theme.accentHover
                             : variant === "ghost" ? (Theme.glassMode ? Qt.rgba(0,0,0,0.10) : Qt.rgba(1,1,1,0.08))
                             : Theme.wallpaperActive ? Theme.btnFillPress
                             : (glassPress.a > 0 ? glassPress : Qt.rgba(1,1,1,0.20))
    property color vBorder: variant === "primary" ? "transparent"
                          : variant === "ghost" ? Theme.glassBorder
                          : Theme.wallpaperActive ? Theme.btnBorder
                          : (borderColor.a > 0 ? borderColor : Qt.rgba(1,1,1,0.12))
    property color vText: variant === "primary" ? "#FFFFFF"
                        : root.enabled ? Theme.text : Theme.textDim

    background: Rectangle {
        radius: btnRadius
        color: !root.enabled ? (Theme.glassMode ? Qt.rgba(0,0,0,0.04) : Qt.rgba(1,1,1,0.03))
             : root.down ? root.vFillPress
             : root.hovered ? root.vFillHover
             : root.vFill
        border.color: root.enabled ? root.vBorder : Qt.rgba(1,1,1,0.06)
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            anchors.fill: parent
            radius: btnRadius
            color: "transparent"
            border.color: Theme.focusRing
            border.width: 1
            visible: root.visible && root.activeFocus
            opacity: 0.9
        }
    }

    contentItem: Text {
        text: root.text
        font: root.font
        color: root.vText
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: root.enabled ? 1 : 0.5
    }
}
