// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Layouts

// Status badge: colored dot + label for persistent operation states.
// state: "ok" | "warn" | "error" | "busy" | "idle"
Rectangle {
    id: root

    property string state: "idle"
    property string label: ""

    implicitHeight: 24
    implicitWidth: labelText.implicitWidth + Theme.sp3 * 2 + 8 + Theme.sp2 + 2
    radius: 12
    color: "transparent"
    border.color: Theme.glassBorder
    border.width: 1

    property color stateColor: {
        switch (root.state) {
        case "ok":    return Theme.ok
        case "warn":  return Theme.warn
        case "error": return Theme.danger
        case "busy":  return Theme.warn
        default:      return Theme.statusIdle   // visible on light glass too
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.sp3
        anchors.rightMargin: Theme.sp3
        spacing: Theme.sp2

        Rectangle {
            Layout.preferredWidth: 8
            Layout.preferredHeight: 8
            radius: 4
            color: root.stateColor
            Behavior on color { ColorAnimation { duration: Theme.durMid } }
            opacity: root.state === "busy" ? 0.5 : 1
            SequentialAnimation on opacity {
                running: root.state === "busy"
                loops: Animation.Infinite
                NumberAnimation { to: 1; duration: 500 }
                NumberAnimation { to: 0.3; duration: 500 }
            }
        }
        Text {
            id: labelText
            text: root.label
            color: Theme.textDim
            font.pixelSize: Theme.fsCaption
        }
    }
}
