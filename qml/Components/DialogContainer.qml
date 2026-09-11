// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Unified dialog container: single consistent chrome for every modal window.
// - dark #141414 background, 16px radius, hairline border
// - standard header: title + subtitle + ghost close button
// - fade open animation, centered, max width 800
//
// NOTE: Dialog.contentItem is FINAL in Qt Quick Controls 2 鈥?it cannot be
// overridden. Instead children are collected via the default property
// `content` and placed into an internal `body` item that fills the dialog's
// default content area (between header and footer).
Dialog {
    id: root

    property string dialogTitle: ""
    property string dialogSubtitle: ""
    property int dialogWidth: 560
    property int dialogHeight: 600

    // children of DialogContainer land here (default property)
    default property alias content: body.data

    width: Math.min(dialogWidth, root.parent ? root.parent.width - 48 : dialogWidth)
    height: Math.min(dialogHeight, root.parent ? root.parent.height - 48 : dialogHeight)
    modal: true
    x: (root.parent ? root.parent.width : 0) / 2 - width / 2
    y: (root.parent ? root.parent.height : 0) / 2 - height / 2
    padding: 0

    enter: Transition {
        ParallelAnimation {
            // opacity = cubic; scale = subtle spring settle (physical weight)
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.durMid; easing.type: Easing.OutCubic }
            SpringAnimation { property: "scale"; from: 0.96; to: 1.0; spring: 4.5; damping: 0.72; epsilon: 0.004 }
        }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.durFast; easing.type: Easing.InCubic }
    }

    background: Rectangle {
        // glass mode: dark frosted at 94% opacity — blocks the bright
        // wallpaper for white-text readability while keeping a subtle
        // glass translucency. Light frosted (cardFill 14%) let the
        // wallpaper bleed through and made text unreadable.
        color: Theme.glassMode ? Qt.rgba(0.14, 0.14, 0.16, 0.94) : Theme.cardFill
        radius: Theme.rXl
        border.color: Theme.glassBorder
        border.width: 1
    }

    header: Rectangle {
        width: root.width
        height: 64
        color: "transparent"

        Column {
            anchors.left: parent.left
            anchors.leftMargin: Theme.sp5
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text {
                text: root.dialogTitle
                color: Theme.text
                font.pixelSize: Theme.fsTitle
                font.bold: true
            }
            Text {
                text: root.dialogSubtitle
                color: Theme.textDim
                font.pixelSize: Theme.fsCaption
                visible: root.dialogSubtitle.length > 0
            }
        }
        AppButton {
            anchors.right: parent.right
            anchors.rightMargin: Theme.sp3
            anchors.verticalCenter: parent.verticalCenter
            text: "✕"
            variant: "ghost"
            implicitWidth: 32
            implicitHeight: 32
            onClicked: root.close()
        }
    }

    // internal content container: fills the dialog's default content area
    Item {
        id: body
        anchors.fill: root.contentItem
        anchors.margins: 0
        clip: true
    }
}
