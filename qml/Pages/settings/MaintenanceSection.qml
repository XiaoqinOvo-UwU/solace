// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../Components"

// 维护：auto-start, update check/download, plugins, privacy toggles.
// Emits notify(message) for the page-level inline note.
SettingsSectionCard {
    id: section

    signal notify(string message)

    title: "维护"

    Text {
        Layout.fillWidth: true
        text: "更新源：GitHub 公开仓库（xiaoqinnb666/xiaoqintools），无需配置令牌"
        color: Theme.textDim
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }

    // auto-start (开机自启) toggle
    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Text {
            text: "开机自启"
            color: Theme.text
            font.pixelSize: 13
            Layout.fillWidth: true
        }
        ThemeSwitch {
            id: autostartSwitch
            checked: sysService.isAutoStartEnabled()
            onToggled: {
                sysService.setAutoStart(checked)
                section.notify(checked ? "已开启开机自启" : "已关闭开机自启")
            }
        }
    }

    RowLayout {
        spacing: 10
        AppButton {
            text: "检查更新"
            Layout.fillWidth: true
            onClicked: {
                section.notify("正在检查更新…")
                updateService.checkForUpdates()
            }
        }
        AppButton {
            text: "导出配置"
            Layout.fillWidth: true
            onClicked: {
                var ok = syncService.exportConfig(syncService.defaultExportPath())
                section.notify(ok ? "配置已导出到杂货铺" : "导出失败")
            }
        }
        AppButton {
            text: "导入配置"
            Layout.fillWidth: true
            onClicked: section.notify("导入配置（文件选择框预留）")
        }
    }

    // update download section (visible when an update is available)
    // theme-consistent inset panel: same dark fill/hairline border
    // as inputs, subtle left accent bar instead of a green wash.
    Rectangle {
        visible: updateService.updateAvailable
        Layout.fillWidth: true
        Layout.preferredHeight: updateCol.implicitHeight + 20
        color: Theme.inputBg
        radius: Theme.rMd
        border.color: Qt.rgba(255,255,255,0.08)
        border.width: 1

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 8
            anchors.bottomMargin: 8
            width: 3
            radius: 1.5
            color: Theme.accent
        }

        ColumnLayout {
            id: updateCol
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 12
            anchors.topMargin: 10
            anchors.bottomMargin: 10
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: "发现新版本 " + updateService.latestVersion + "（当前 " + updateService.currentVersion() + "）"
                color: Theme.text
                font.pixelSize: 13
                font.bold: true
                wrapMode: Text.Wrap
            }
            ProgressBar {
                id: updateBar
                Layout.fillWidth: true
                visible: updateService.downloading
                from: 0; to: 100
                value: updateService.downloadProgress
                // while the speed probe / first bytes arrive
                // (0% known yet) show an animated indeterminate
                // sweep so the bar never looks frozen or full.
                indeterminate: updateService.downloading && updateService.downloadProgress <= 0
                height: 6
                // NOTE: Qt6 does NOT auto-scale a custom
                // contentItem width to visualPosition — it gets
                // the FULL bar width (the bar was all-green).
                // Render the fill ourselves against the track.
                background: Rectangle { radius: 3; color: Theme.sliderTrack }
                contentItem: Item {
                    Rectangle {
                        width: parent.width * updateBar.visualPosition
                        height: parent.height
                        radius: 3
                        color: Theme.sliderFill
                        Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                AppButton {
                    text: "下载并安装"
                    Layout.fillWidth: true
                    enabled: !updateService.downloading
                    onClicked: updateService.downloadAndInstall()
                }
                AppButton {
                    text: "忽略"
                    Layout.fillWidth: true
                    onClicked: appCore.showToast("已忽略本次更新")
                }
            }
        }
    }
    Text {
        text: "已发现插件：" + pluginManager.listPlugins().join("、")
        color: Theme.textDim
        font.pixelSize: 12
    }

    // ---- privacy toggles ----
    Text { text: "隐私与陪伴"; color: Theme.text; font.pixelSize: 15; font.bold: true; Layout.topMargin: 8 }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Text {
            Layout.fillWidth: true
            text: "允许 AI 读取电脑状态（当前应用/空闲/电量）"
            color: Theme.text
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
        ThemeSwitch {
            id: stateSwitch
            checked: aiService.allowStateRead()
            onToggled: {
                aiService.setAllowStateRead(checked)
                section.notify(checked ? "已开启状态感知" : "已关闭状态感知")
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Text {
            Layout.fillWidth: true
            text: "允许记录使用时长与习惯"
            color: Theme.text
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
        ThemeSwitch {
            id: timeSwitch
            checked: aiService.allowTimeRecord()
            onToggled: {
                aiService.setAllowTimeRecord(checked)
                section.notify(checked ? "已开启时长记录" : "已关闭时长记录")
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        Text {
            Layout.fillWidth: true
            text: "允许长期记忆（AI 记住共同经历）"
            color: Theme.text
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
        ThemeSwitch {
            id: memSwitch
            checked: aiService.allowLongTermMemory()
            onToggled: {
                aiService.setAllowLongTermMemory(checked)
                section.notify(checked ? "已开启长期记忆" : "已关闭长期记忆")
            }
        }
    }

    Connections {
        target: updateService
        // NO islands for updates (user rule). Everything is reflected inline:
        // the panel above when an update exists, plus an inline note for errors.
        // This also runs for the hourly BACKGROUND check — must stay silent.
        function onCheckFinished(ok) {
            if (updateService.lastError.length > 0)
                section.notify(updateService.lastError)
        }
        function onDownloadFinished(ok, message) {
            section.notify(message)
        }
    }
}
