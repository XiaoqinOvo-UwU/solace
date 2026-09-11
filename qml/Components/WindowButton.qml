// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls

// Frameless-window control button (Telegram style):
//   default   — transparent cell, icon rgba(255,255,255,0.5)
//   hover     — 20% grey rounded fill, icon brightens
//   close     — hover turns a LOW-SATURATION red (never a white block)
Item {
    id: root

    property string variant: "close"          // min | max | restore | close
    property string tip: ""

    width: 46
    height: 36

    readonly property bool danger: root.variant === "close"
    readonly property color iconColor: root.hov ? (danger ? "#FFFFFF" : Qt.rgba(1,1,1,0.9))
                                               : Qt.rgba(1,1,1,0.5)

    // rounded hover fill, inset 4px — subtle DARK grey, never a white block
    Rectangle {
        anchors.fill: parent
        anchors.margins: 4
        radius: 6
        color: root.dwn ? (danger ? Qt.rgba(190/255,70/255,70/255,0.72) : Qt.rgba(66/255,66/255,66/255,0.9))
             : root.hov ? (danger ? Qt.rgba(190/255,70/255,70/255,0.62) : Qt.rgba(56/255,56/255,56/255,0.9))
             : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.durFast; easing.type: Easing.OutCubic } }
    }

    // ---- drawn icons ----
    Item {
        id: ico
        width: 16
        height: 16
        anchors.centerIn: parent
        opacity: root.hov ? 1.0 : 0.85
        Behavior on opacity { NumberAnimation { duration: Theme.durFast } }

        // minimize: a horizontal line
        Rectangle {
            visible: root.variant === "min"
            width: 14; height: 1.6; radius: 0.8
            anchors.centerIn: parent
            color: root.iconColor
        }

        // maximize: an outlined square
        Rectangle {
            visible: root.variant === "max"
            width: 13; height: 13
            anchors.centerIn: parent
            color: "transparent"
            border.color: root.iconColor
            border.width: 1.5
            radius: 1.5
        }

        // restore: two overlapping squares
        Rectangle {
            visible: root.variant === "restore"
            width: 12; height: 12
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 2.5
            anchors.verticalCenterOffset: -2.5
            color: "transparent"
            border.color: root.iconColor
            border.width: 1.5
            radius: 1
        }
        Rectangle {
            visible: root.variant === "restore"
            width: 12; height: 12
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: -2.5
            anchors.verticalCenterOffset: 2.5
            color: root.danger ? Qt.rgba(185/255,65/255,65/255,0.5) : Qt.rgba(0,0,0,0.4)
            border.color: root.iconColor
            border.width: 1.5
            radius: 1
        }

        // close: an X drawn from two crossing bars
        Item {
            visible: root.variant === "close"
            anchors.centerIn: parent
            width: 14; height: 14
            Rectangle {
                width: 15; height: 1.7; radius: 0.85
                anchors.centerIn: parent
                color: root.iconColor
                rotation: 45
            }
            Rectangle {
                width: 15; height: 1.7; radius: 0.85
                anchors.centerIn: parent
                color: root.iconColor
                rotation: -45
            }
        }
    }

    // explicit hover/press state
    property bool hov: false
    property bool dwn: false

    ToolTip.visible: root.hov && root.tip.length > 0
    ToolTip.text: root.tip
    ToolTip.delay: 600

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        onEntered: root.hov = true
        onExited: { root.hov = false; root.dwn = false }
        onPressed: root.dwn = true
        onReleased: { root.dwn = false; root.hov = area.containsMouse }
        onClicked: root.clicked()
    }

    signal clicked()
}
