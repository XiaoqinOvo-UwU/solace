// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick

// Chat message bubble (Telegram-style): AI left with avatar, user right with
// avatar. Uniform rXl radius per design-system (cards), hairline border on the
// AI bubble. Supports an animated three-dot "typing" state.
Rectangle {
    id: root

    property bool isAi: true
    property string text: ""
    property string time: ""
    property bool typing: false            // show animated dots instead of text
    property int bubbleMax: 300            // hard cap passed by the delegate

    readonly property int txtPad: 14       // inner horizontal padding

    // natural single-line text width, measured by an UNCONSTRAINED invisible
    // Text. Using the visible (width-bound, wrapping) txt.implicitWidth would
    // report the WRAPPED width once text wraps — a feedback loop that shrinks
    // bubbles and wraps short messages early. This measurer is stable.
    Text {
        id: measurer
        visible: false
        text: root.text
        font.pixelSize: Theme.fsBody
    }
    readonly property int contentWidth: root.typing ? 0 : measurer.implicitWidth

    // ---- sizing (caller sets `width` from contentWidth) ----
    implicitWidth: root.typing ? 64 : contentWidth + txtPad * 2
    implicitHeight: Math.max(36, (root.typing ? dotsRow.height : txt.implicitHeight) + 18)

    radius: Theme.rXl
    // AI bubble: light frosted glass on glass mode (Theme.aiBubbleFill).
    // User bubble: opaque theme token (muted indigo) — its text is white.
    color: isAi ? Theme.aiBubbleFill : Theme.userBubbleFill
    border.color: isAi ? Theme.glassBorder : "transparent"
    border.width: 1

    // message text
    Text {
        id: txt
        anchors.fill: parent
        anchors.margins: root.txtPad
        visible: !root.typing
        text: root.text
        color: isAi ? Theme.text : Theme.onUserBubble
        font.pixelSize: Theme.fsBody
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
        verticalAlignment: Text.AlignVCenter
    }

    // ---- typing indicator: three dots that pulse in sequence ----
    Row {
        id: dotsRow
        anchors.centerIn: parent
        spacing: 4
        visible: root.typing
        property int phase: 0
        Timer {
            interval: 220
            repeat: true
            running: root.typing
            onTriggered: dotsRow.phase = (dotsRow.phase + 1) % 3
        }
        Repeater {
            model: 3
            Rectangle {
                width: 6; height: 6; radius: 3
                color: Theme.textDim
                opacity: dotsRow.phase === index ? 1 : 0.35
                scale: dotsRow.phase === index ? 1.2 : 1.0
                Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                Behavior on scale { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            }
        }
    }

    // time label (unused for now; kept for API compatibility)
    Text {
        visible: !root.typing && root.time.length > 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 2
        anchors.right: root.isAi ? parent.left : parent.right
        anchors.rightMargin: 8
        text: root.time
        color: Theme.textDim
        font.pixelSize: Theme.fsCaption
    }
}
