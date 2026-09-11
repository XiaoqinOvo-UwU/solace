// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls

// Thin themed scrollbar — replaces Qt's default light bar. Attach via
// `ScrollBar.vertical: AppScrollBar {}` on a ScrollView / Flickable.
ScrollBar {
    id: root

    policy: ScrollBar.AsNeeded

    readonly property color _handle: root.pressed
        ? (Theme.glassMode ? Qt.rgba(0, 0, 0, 0.45) : Theme.textMuted)
        : root.hovered
        ? (Theme.glassMode ? Qt.rgba(0, 0, 0, 0.35) : Theme.textDim)
        : (Theme.glassMode ? Qt.rgba(0, 0, 0, 0.22) : Qt.rgba(1, 1, 1, 0.20))

    contentItem: Rectangle {
        implicitWidth: 6
        radius: 3
        color: root._handle
        Behavior on color { ColorAnimation { duration: Theme.durFast } }
    }

    background: Item {}
}
