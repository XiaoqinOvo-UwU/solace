// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Sidebar navigation item: leading glyph + label + selected indicator bar.
// Keyboard focusable (Enter/Space activates), hover + selected states.
Rectangle {
    id: root

    property string iconText: ""
    property string label: ""
    property bool selected: false
    property bool hovered: false
    signal clicked()

    implicitHeight: 40
    color: selected ? Theme.selected
         : hovered ? Theme.hoverBg
         : "transparent"
    radius: Theme.rMd
    Behavior on color { ColorAnimation { duration: Theme.durMid } }
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: root.label

    // selected indicator (left bar)
    Rectangle {
        width: 3; height: selected ? 20 : 0
        anchors.verticalCenter: parent.verticalCenter
        anchors.left: parent.left
        anchors.leftMargin: 2
        color: Theme.navText
        radius: 1.5
        opacity: selected ? 1 : 0
        Behavior on height { NumberAnimation { duration: Theme.durMid; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: Theme.durMid } }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.sp3
        anchors.rightMargin: Theme.sp3
        spacing: Theme.sp3
        Text {
            text: root.iconText
            color: selected ? Theme.navText : Theme.navTextDim
            font.pixelSize: Theme.fsDefault
            Layout.preferredWidth: 20
            opacity: selected ? 1 : 0.75
            Behavior on opacity { NumberAnimation { duration: Theme.durMid } }
        }
        Text {
            text: root.label
            color: selected ? Theme.navText : Theme.navTextDim
            font.pixelSize: Theme.fsDefault
            font.bold: selected
        }
        Item { Layout.fillWidth: true }
    }

    // focus ring (a11y) — outside the RowLayout, anchored to the item itself
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        radius: Theme.rMd
        border.color: Theme.focusRing
        border.width: 1
        visible: root.activeFocus
        opacity: 0.9
    }

    function activate() { root.clicked() }
    Keys.onReturnPressed: activate()
    Keys.onEnterPressed: activate()
    Keys.onSpacePressed: activate()

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: root.hovered = true
        onExited: root.hovered = false
        onClicked: root.clicked()
    }
}
