// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../Components"

// 外观模式：默认深色 vs 白色玻璃（wallpaper glass）。Owns the appearance-mode
// state and applies it to the Theme singleton in real time + persists it.
SettingsSectionCard {
    id: section

    // current wallpaper url — only used for the "set a wallpaper first" hint
    property string wallpaperUrl: ""
    property string appMode: ""        // "" 默认深色 | "glass" 壁纸玻璃

    contentSpacing: 12

    Component.onCompleted: {
        section.appMode = aiService.appearanceMode()
        Theme.appearanceMode = section.appMode
    }

    // real-time appearance switch: update the Theme singleton (every token
    // re-renders) + persist via AiService.
    function applyAppearance(mode) {
        section.appMode = (mode === "glass") ? "glass" : ""
        Theme.appearanceMode = section.appMode
        aiService.setAppearanceMode(section.appMode)
    }

    title: "外观模式"

    Text {
        Layout.fillWidth: true
        text: "白色玻璃：UI 变成低亮度白色磨砂玻璃悬浮在壁纸上（Apple Acrylic / VisionOS 风格），低调不抢视觉"
        color: Theme.textDim
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        // 默认深色
        Rectangle {
            id: modeDark
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            radius: Theme.rMd
            color: section.appMode === "" ? Qt.rgba(255,255,255,0.14)
                                          : Qt.rgba(255,255,255,0.05)
            border.color: section.appMode === "" ? Theme.ok : Qt.rgba(255,255,255,0.10)
            border.width: 1
            Behavior on color { ColorAnimation { duration: Theme.durFast } }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                Text {
                    text: "默认深色"
                    color: Theme.text
                    font.pixelSize: 13
                    font.bold: section.appMode === ""
                    Layout.fillWidth: true
                }
                Text {
                    text: section.appMode === "" ? "✓" : ""
                    color: Theme.ok
                    font.pixelSize: 14
                    font.bold: true
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: section.applyAppearance("")
            }
        }
        // 壁纸玻璃
        Rectangle {
            id: modeGlass
            Layout.fillWidth: true
            Layout.preferredHeight: 46
            radius: Theme.rMd
            color: section.appMode === "glass" ? Qt.rgba(255,255,255,0.14)
                                               : Qt.rgba(255,255,255,0.05)
            border.color: section.appMode === "glass" ? Theme.ok : Qt.rgba(255,255,255,0.10)
            border.width: 1
            Behavior on color { ColorAnimation { duration: Theme.durFast } }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                Text {
                    text: "白色玻璃"
                    color: Theme.text
                    font.pixelSize: 13
                    font.bold: section.appMode === "glass"
                    Layout.fillWidth: true
                }
                Text {
                    text: section.appMode === "glass" ? "✓" : ""
                    color: Theme.ok
                    font.pixelSize: 14
                    font.bold: true
                }
            }
            MouseArea {
                anchors.fill: parent
                onClicked: section.applyAppearance("glass")
            }
        }
    }

    Text {
        Layout.fillWidth: true
        visible: section.appMode === "glass" && section.wallpaperUrl.length === 0
        text: "提示：请先在「自定义壁纸」里设置壁纸，玻璃效果才会显现"
        color: Theme.warn
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }
}
