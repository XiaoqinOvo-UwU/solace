// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Effects

// Reusable frosted-glass backdrop for dialogs and popups: samples an item that
// sits BEHIND the dialog, blurs it, and clips the result to a rounded shape so
// whatever is behind reads as out-of-focus glass instead of a flat fill.
//
// The capture is not live — a modal dialog covers its backdrop, so a single
// fresh grab when it opens is enough (and far cheaper than a live source).
Item {
    id: root

    // item behind the dialog; must NOT be an ancestor of the dialog itself
    property Item backdropSource: null
    property real cornerRadius: Theme.rXl
    property real blurAmount: 1.0      // 0..1 — MultiEffect blur strength
    property int blurMax: 80           // max blur radius in px

    // re-grab the backdrop (call from the dialog's onOpened)
    function update() {
        capture.scheduleUpdate()
    }

    ShaderEffectSource {
        id: capture
        visible: false
        live: false
        hideSource: false
        sourceItem: root.backdropSource
        sourceRect: {
            if (!root.backdropSource)
                return Qt.rect(0, 0, 0, 0)
            const p = root.mapToItem(root.backdropSource, 0, 0)
            return Qt.rect(p.x, p.y, root.width, root.height)
        }
    }

    // rounded mask for the blurred plate (MultiEffect has no clip of its own)
    Rectangle {
        id: maskShape
        anchors.fill: parent
        radius: root.cornerRadius
        color: "black"
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: capture
        visible: root.backdropSource !== null
        autoPaddingEnabled: false
        blurEnabled: true
        blur: root.blurAmount
        blurMax: root.blurMax
        maskEnabled: true
        maskSource: maskShape
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
