// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Shared section-card chrome for settings groups: one raised surface +
// hairline border + uniform 18px padding. Content is injected through the
// default property; the title renders above it when non-empty.
Rectangle {
    id: card

    property string title: ""
    property real contentSpacing: 10
    default property alias content: col.data

    Layout.fillWidth: true
    implicitHeight: col.implicitHeight + 36
    color: Theme.cardFill
    radius: 12
    border.color: Qt.rgba(255,255,255,0.10)
    border.width: 1

    ColumnLayout {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        anchors.topMargin: 18
        spacing: card.contentSpacing

        Text {
            visible: card.title.length > 0
            text: card.title
            color: Theme.text
            font.pixelSize: 15
            font.bold: true
        }
    }
}
