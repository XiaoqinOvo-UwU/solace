// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Unified page header: title + subtitle, always top-aligned at the same Y.
// Must NOT vertical-center — if ColumnLayout stretches height, center would
// push the title down (the "偏下" bug on Network/System pages).
Item {
    id: root

    property string title: ""
    property string subtitle: ""

    // fixed slot so every page's headline starts at the same Y
    Layout.fillWidth: true
    Layout.preferredHeight: 40
    Layout.maximumHeight: 40
    Layout.minimumHeight: 40
    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
    implicitHeight: 40
    implicitWidth: 200

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 2
        Text {
            text: root.title
            color: Theme.text
            font.pixelSize: Theme.fsPage
            font.bold: true
        }
        Text {
            text: root.subtitle
            color: Theme.textDim
            font.pixelSize: Theme.fsSmall
        }
    }
}
