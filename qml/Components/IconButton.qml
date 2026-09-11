// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls

// Icon-only button: square glyph button with hover/press/focus states.
// `glyph` is a single unicode character (not emoji) or short text label.
Button {
    id: root

    property string glyph: ""
    property string tip: ""            // tooltip
    property int btnSize: 32

    implicitWidth: btnSize
    implicitHeight: btnSize
    focusPolicy: Qt.StrongFocus
    font.pixelSize: Theme.fsDefault

    // a11y: glyph-only, so the tooltip is the only human-readable name
    Accessible.role: Accessible.Button
    Accessible.name: root.tip.length > 0 ? root.tip : root.glyph

    ToolTip.visible: root.hovered && root.tip.length > 0
    ToolTip.text: root.tip
    ToolTip.delay: 600

    background: Rectangle {
        radius: Theme.rMd
        color: !root.enabled ? Qt.rgba(1,1,1,0.03)
             : root.down ? Qt.rgba(1,1,1,0.20)
             : root.hovered ? Qt.rgba(1,1,1,0.12)
             : Qt.rgba(1,1,1,0.06)
        border.color: Qt.rgba(1,1,1,0.10)
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.durFast } }

        Rectangle {
            anchors.fill: parent
            radius: Theme.rMd
            color: "transparent"
            border.color: Theme.focusRing
            border.width: 1
            visible: root.activeFocus
            opacity: 0.9
        }
    }

    contentItem: Text {
        text: root.glyph
        font: root.font
        color: root.enabled ? Theme.text : Theme.textDim
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
