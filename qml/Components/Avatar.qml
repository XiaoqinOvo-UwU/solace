// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Reusable circular avatar: image or fallback character.
// Sizes: 28 (list), 40 (header), 64 (profile).
Rectangle {
    id: root

    property string source: ""          // image path (file:// or empty for char)
    property string charText: "A"       // fallback char when no image
    property int size: 40
    property bool pressable: false
    signal clicked()

    width: size
    height: size
    radius: size / 2
    color: Theme.accent
    clip: true
    // antialiasing smooths the circular crop edge; mipmap keeps the image
    // crisp when a large source is scaled DOWN to a small avatar
    antialiasing: true
    smooth: true
    border.color: Theme.glassBorder
    border.width: 1
    scale: pressArea.pressed ? 0.92 : 1.0
    Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Easing.OutCubic } }

    Image {
        anchors.fill: parent
        visible: root.source.length > 0
        source: root.source
        fillMode: Image.PreserveAspectCrop
        smooth: true
        mipmap: true
    }
    Text {
        anchors.centerIn: parent
        visible: root.source.length === 0
        text: root.charText.length > 0 ? root.charText.charAt(0) : "A"
        color: "white"
        font.pixelSize: root.size * 0.42
        font.bold: true
    }
    MouseArea {
        id: pressArea
        anchors.fill: parent
        visible: root.pressable
        hoverEnabled: true
        onClicked: root.clicked()
    }
}
