// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls

// Themed tooltip — replaces Qt's default light popup that clashed with both the
// dark theme and white glass. Used as a child: AppToolTip { visible: ...; text: ... }
ToolTip {
    id: root

    delay: 600

    background: Rectangle {
        color: Theme.islandBg
        radius: Theme.rSm
        border.color: Theme.islandBorder
        border.width: 1
    }

    contentItem: Text {
        text: root.text
        color: Theme.islandText
        font.pixelSize: Theme.fsSmall
    }
}
