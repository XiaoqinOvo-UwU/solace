// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "../../Components"

// 自定义壁纸：built-in gradient presets, custom image upload, blur controls.
// Owns the wallpaper preview state; exposes previewUrl so sibling sections
// (e.g. appearance mode hint) can react without touching services.
SettingsSectionCard {
    id: section

    readonly property string previewUrl: wpPreviewUrl

    property string wpPreviewUrl: ""   // current wallpaper thumbnail (file:// url)
    property int wpPreset: -1          // which built-in preset is active (-1 = custom/none)

    contentSpacing: 12

    Component.onCompleted: {
        section.wpPreviewUrl = aiService.wallpaperPath()
    }
    Connections {
        target: aiService
        function onWallpaperChanged() { section.wpPreviewUrl = aiService.wallpaperPath() }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 12
        Text {
            text: "自定义壁纸"
            color: Theme.text; font.pixelSize: 15; font.bold: true
            Layout.fillWidth: true
        }
        // thumbnail preview of the current wallpaper
        Rectangle {
            width: 72; height: 44
            radius: 8
            color: Theme.inputBg
            border.color: Qt.rgba(255,255,255,0.10)
            border.width: 1
            clip: true
            Image {
                anchors.fill: parent
                visible: section.wpPreviewUrl.length > 0
                source: section.wpPreviewUrl
                fillMode: Image.PreserveAspectCrop
                smooth: true
                mipmap: true
            }
            Text {
                anchors.centerIn: parent
                visible: section.wpPreviewUrl.length === 0
                text: "无"
                color: Theme.textDim
                font.pixelSize: 12
            }
        }
    }

    Text {
        Layout.fillWidth: true
        text: "壁纸只作为右侧面板的背景层，UI 保持深色、文字始终清晰。"
        color: Theme.textDim
        font.pixelSize: 12
        wrapMode: Text.Wrap
    }

    // ---- built-in preset wallpapers (dark gradients) ----
    Text {
        text: "内置壁纸"
        color: Theme.text; font.pixelSize: 13; font.bold: true
        Layout.topMargin: 2
    }
    Row {
        Layout.fillWidth: true
        spacing: 8
        Repeater {
            model: [
                { n: "炭黑",  c0: "#26282C", c1: "#0E0F11" },
                { n: "石墨灰", c0: "#2B2C30", c1: "#101113" },
                { n: "墨绿",  c0: "#22302B", c1: "#0C100E" },
                { n: "深青",  c0: "#1F2E33", c1: "#0B0F11" }
            ]
            Rectangle {
                id: tile
                readonly property int idx: index
                property bool hov: false
                width: 66; height: 44
                radius: Theme.rMd
                gradient: Gradient {
                    GradientStop { position: 0.0; color: modelData.c0 }
                    GradientStop { position: 1.0; color: modelData.c1 }
                }
                border.color: section.wpPreset === tile.idx ? Theme.ok
                            : tile.hov ? Qt.rgba(255,255,255,0.35)
                            : Qt.rgba(255,255,255,0.10)
                border.width: section.wpPreset === tile.idx ? 2 : 1
                scale: tile.hov ? 1.06 : 1.0
                Behavior on scale { NumberAnimation { duration: Theme.durFast; easing.type: Easing.OutCubic } }
                Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.n
                    color: "white"
                    font.pixelSize: 10
                    opacity: 0.85
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: tile.hov = true
                    onExited: tile.hov = false
                    onClicked: {
                        aiService.setWallpaperPreset(tile.idx)
                        section.wpPreset = tile.idx
                        section.wpPreviewUrl = aiService.wallpaperPath()
                        appCore.showToast("壁纸已应用~")
                    }
                }
            }
        }
    }

    Text {
        text: "或上传自定义图片"
        color: Theme.textDim
        font.pixelSize: 12
        Layout.topMargin: 4
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        AppButton {
            text: "选择图片"
            implicitHeight: 34
            onClicked: wallpaperFileDialog.open()
        }
        AppButton {
            text: "移除壁纸"
            variant: "ghost"
            implicitHeight: 34
            visible: section.wpPreviewUrl.length > 0
            onClicked: {
                aiService.removeWallpaper()
                section.wpPreset = -1
                section.wpPreviewUrl = aiService.wallpaperPath()
            }
        }
        Item { Layout.fillWidth: true }
    }

    // ---- blur control: on/off + strength ----
    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: Qt.rgba(255,255,255,0.08)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Column {
            Layout.fillWidth: true
            spacing: 2
            Text { text: "模糊背景"; color: Theme.text; font.pixelSize: 13; font.bold: true }
            Text {
                text: section.wpPreviewUrl.length > 0 ? "关闭后直接显示原图" : "先选择壁纸后生效"
                color: Theme.textDim; font.pixelSize: 11
            }
        }
        ThemeSwitch {
            id: wpBlurSwitch
            checked: aiService.wallpaperBlurEnabled()
            onToggled: aiService.setWallpaperBlurEnabled(checked)
        }
    }

    RowLayout {
        Layout.fillWidth: true
        visible: wpBlurSwitch.checked
        spacing: 10
        Text {
            text: "模糊强度"
            color: Theme.text
            font.pixelSize: 13
        }
        DarkSlider {
            id: wpBlurSlider
            Layout.fillWidth: true
            from: 0; to: 40; stepSize: 1
            value: aiService.wallpaperBlurRadius()
            onMoved: aiService.setWallpaperBlurRadius(value)
        }
        Text {
            text: wpBlurSlider.value
            color: Theme.textDim
            font.pixelSize: 12
            font.features: { "tnum": 1 }
            Layout.preferredWidth: 24
            horizontalAlignment: Text.AlignRight
        }
    }

    FileDialog {
        id: wallpaperFileDialog
        title: "选择壁纸"
        nameFilters: ["图片 (*.png *.jpg *.jpeg *.bmp *.webp)"]
        onAccepted: {
            var path = wallpaperFileDialog.selectedFile.toString()
            if (path.indexOf("file:///") === 0) path = path.substring(8)
            else if (path.indexOf("file://") === 0) path = path.substring(7)
            path = decodeURIComponent(path)
            aiService.setWallpaper(path)
            section.wpPreset = -1
            section.wpPreviewUrl = aiService.wallpaperPath()
            appCore.showToast("壁纸已应用~")
        }
    }
}
