// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls

// Dark custom switch (design system): pill track + sliding handle, green "on"
// state (Theme.ok), neutral grey "off". Smooth motion + visible focus ring.
Switch {
    id: root

    implicitWidth: 40
    implicitHeight: 22
    opacity: root.enabled ? 1 : 0.5

    indicator: Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 40; height: 22
        radius: 11
        color: root.checked ? Theme.ok
                            : Theme.hoverBgStrong
        border.color: Qt.rgba(255,255,255,0.10)
        border.width: 1
        Behavior on color { ColorAnimation { duration: Theme.durMid; easing.type: Easing.OutCubic } }

        Rectangle {
            width: 16; height: 16; radius: 8
            color: "#E8E8E8"
            antialiasing: true
            x: root.checked ? root.indicator.width - width - 3 : 3
            y: (root.indicator.height - height) / 2
            Behavior on x { NumberAnimation { duration: Theme.durMid; easing.type: Easing.OutCubic } }
        }
    }

    // focus ring (a11y keyboard navigation)
    Rectangle {
        anchors.fill: parent
        radius: 11
        color: "transparent"
        border.color: Theme.focusRing
        border.width: 1
        visible: root.activeFocus
        opacity: 0.9
    }
}
