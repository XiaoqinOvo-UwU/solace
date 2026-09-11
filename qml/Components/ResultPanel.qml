// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Result panel: monospace-ish output area inside a card, used next to the
// operation that produced it. Content auto-grows via plain Text (no inner
// ScrollView, no TextArea — avoids nested-wheel conflicts and layout
// collapse inside ColumnLayout). The page's outer ScrollView scrolls.
Rectangle {
    id: root

    property string title: ""          // e.g. "测速结果"
    property string text: ""           // content
    property bool busy: false          // show "进行中…" instead of content
    property string emptyHint: ""      // shown when text is empty and idle

    color: Theme.cardFill
    radius: Theme.rLg
    border.color: Theme.glassBorder
    border.width: 1
    clip: true

    Accessible.role: Accessible.StaticText
    Accessible.name: root.title

    // height = inner ColumnLayout implicit height + vertical margins
    // (anchors.margins are not included in the layout's implicit size) —
    // matches the real layout exactly, so nothing gets clipped
    implicitHeight: body.implicitHeight + Theme.sp3 * 2

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: Theme.sp3
        spacing: Theme.sp2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.sp2
            Text {
                text: root.title
                color: Theme.textDim
                font.pixelSize: Theme.fsSmall
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Text {
                text: root.busy ? "进行中…" : ""
                color: Theme.warn
                font.pixelSize: Theme.fsCaption
                visible: root.busy
            }
        }

        // content box: a refined inset — darker than the card, with a hairline
        // border — not the washed default input fill (the "cheap" look)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: outText.implicitHeight + Theme.sp3 * 2
            color: Theme.outputBg
            radius: Theme.rMd
            border.color: Theme.outputBorder
            border.width: 1
            visible: !root.busy && root.text.length > 0

            Text {
                id: outText
                // width fixed by parent; height = content (no anchor cycle)
                width: parent.width
                height: implicitHeight
                color: Theme.text
                font.pixelSize: Theme.fsSmall
                font.family: "Consolas"
                lineHeight: 1.35
                wrapMode: Text.Wrap
                text: root.text
                padding: Theme.sp3
                // fade-in on new results (state change feedback)
                opacity: 1
                Behavior on opacity { NumberAnimation { duration: Theme.durMid } }
                onTextChanged: { opacity = 0.3; Qt.callLater(function() { opacity = 1 }) }
            }
        }

        // empty / busy hint (no content box yet)
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.outputBg
            radius: Theme.rMd
            border.color: Theme.outputBorder
            border.width: 1
            visible: root.busy || root.text.length === 0
            Text {
                anchors.centerIn: parent
                text: root.busy ? "检测中…" : root.emptyHint
                color: Theme.textDim
                font.pixelSize: Theme.fsSmall
                // subtle "alive" pulse while a background operation runs
                SequentialAnimation on opacity {
                    running: root.busy
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.45; duration: 700; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0;  duration: 700; easing.type: Easing.InOutSine }
                }
            }
        }
    }
}
