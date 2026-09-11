// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Components"

// System tools: grouped sections (cleanup / analysis / startup).
Page {
    id: page
    padding: 0
    background: Rectangle { color: "transparent" }

    Connections {
        target: sysService
        function onCleanupDone(message) {
            appCore.showToast(message)
            var m = message.match(/释放\s*(\d+)\s*MB/)
            if (m) statsService.record("clean", m[1])
        }
    }

    // fixed header + scrollable body (same layout as Settings page)
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.sp5
        spacing: Theme.sp4

        PageHeader {
            title: "系统工具"
            subtitle: "帮你把电脑收拾得干干净净"
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical.policy: ScrollBar.AsNeeded
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.sp4

            // ================= 分区① 系统清理 =================
            SectionLabel { text: "系统清理" }
            GridLayout {
                Layout.leftMargin: Theme.sp1
                Layout.rightMargin: Theme.sp1
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.sp5
                rowSpacing: Theme.sp5

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.sp4
                        spacing: Theme.sp3
                        Rectangle {
                            width: 40; height: 40; radius: Theme.rLg
                            color: Qt.rgba(1,1,1,0.06)
                            border.color: Theme.glassBorder
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "清"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fsDefault
                                font.bold: true
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: "清理垃圾"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "临时文件 / 缓存 / 崩溃转储"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        Text { text: "›"; color: Theme.textDim; font.pixelSize: Theme.fsDefault }
                    }
                    onClicked: {
                        appCore.showToast("泉此方开始扫垃圾了...")
                        sysService.cleanJunkAsync()
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.sp4
                        spacing: Theme.sp3
                        Rectangle {
                            width: 40; height: 40; radius: Theme.rLg
                            color: Qt.rgba(1,1,1,0.06)
                            border.color: Theme.glassBorder
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "内"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fsDefault
                                font.bold: true
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: "清理内存"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "释放进程内存"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        Text { text: "›"; color: Theme.textDim; font.pixelSize: Theme.fsDefault }
                    }
                    onClicked: {
                        appCore.showToast(sysService.cleanMemory())
                        statsService.record("mem", "1")
                    }
                }
            }

            // ================= 分区② 分析与报告 =================
            SectionLabel { text: "分析与报告" }
            GridLayout {
                Layout.leftMargin: Theme.sp1
                Layout.rightMargin: Theme.sp1
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.sp5
                rowSpacing: Theme.sp5

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.sp4
                        spacing: Theme.sp3
                        Rectangle {
                            width: 40; height: 40; radius: Theme.rLg
                            color: Qt.rgba(1,1,1,0.06)
                            border.color: Theme.glassBorder
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "报"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fsDefault
                                font.bold: true
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: "使用报告"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "抽卡 / 心情 / 清理统计"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        Text { text: "›"; color: Theme.textDim; font.pixelSize: Theme.fsDefault }
                    }
                    onClicked: {
                        reportDialog.tab = 0
                        reportDialog.loadData()
                        reportDialog.open()
                    }
                }

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.sp4
                        spacing: Theme.sp3
                        Rectangle {
                            width: 40; height: 40; radius: Theme.rLg
                            color: Qt.rgba(1,1,1,0.06)
                            border.color: Theme.glassBorder
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "扫"
                                color: Theme.textMuted
                                font.pixelSize: Theme.fsDefault
                                font.bold: true
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: "大文件扫描"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "找出 C 盘的大文件"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        Text { text: "›"; color: Theme.textDim; font.pixelSize: Theme.fsDefault }
                    }
                    onClicked: {
                        toolResult.text = sysService.scanLargeFiles()
                        toolDialog.title = "大文件扫描"
                        toolDialog.open()
                    }
                }
            }

            // ================= 分区③ 启动管理 =================
            SectionLabel { text: "启动管理" }
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 84
                clickable: true
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp4
                    spacing: Theme.sp3
                    Rectangle {
                        width: 40; height: 40; radius: Theme.rLg
                        color: Qt.rgba(1,1,1,0.06)
                        border.color: Theme.glassBorder
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: "启"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fsDefault
                            font.bold: true
                        }
                    }
                    Column {
                        Layout.fillWidth: true
                        spacing: 2
                        Text { text: "开机自启管理"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "查看 / 禁用启动项"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                    Text { text: "›"; color: Theme.textDim; font.pixelSize: Theme.fsDefault }
                }
                onClicked: {
                    startupModel.clear()
                    var items = sysService.startupItemList()
                    for (var i = 0; i < items.length; i++) {
                        var parts = items[i].split("|")
                        if (parts.length >= 2) {
                            startupModel.append({ "name": parts[0], "cmd": parts[1], "src": parts.length > 2 ? parts[2] : "用户" })
                        }
                    }
                    startupDialog.open()
                }
            }

            Item { Layout.preferredHeight: Theme.sp6 }
            }
        }
    }

    Dialog {
        id: startupDialog
        width: 560
        height: 460
        modal: true
        anchors.centerIn: Overlay.overlay
        title: "开机自启项"
        background: Rectangle {
            color: Theme.cardFill
            radius: 12
            border.color: Theme.glassBorder
            border.width: 1
        }
        contentItem: Column {
            spacing: 8

            ListModel { id: startupModel }

            ListView {
                id: startupView
                width: parent.width
                height: 330
                clip: true
                spacing: 4
                model: startupModel
                delegate: Rectangle {
                    width: startupView.width
                    height: 44
                    radius: 8
                    color: startupView.currentIndex === index ? Theme.selected : Theme.glass
                    border.color: Theme.glassBorder
                    border.width: 1
                    MouseArea {
                        anchors.fill: parent
                        onClicked: startupView.currentIndex = index
                    }
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: model.name
                                color: Theme.text
                                font.pixelSize: 13
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: model.src + " · " + model.cmd
                                color: Theme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }

            RowLayout {
                width: parent.width
                spacing: 8
                AppButton {
                    text: "禁用所选"
                    Layout.fillWidth: true
                    enabled: startupView.currentIndex >= 0
                    onClicked: {
                        var it = startupModel.get(startupView.currentIndex)
                        sysService.disableStartupItem(it.name)
                        startupModel.remove(startupView.currentIndex)
                    }
                }
                AppButton {
                    text: "关闭"
                    Layout.fillWidth: true
                    onClicked: startupDialog.close()
                }
            }
        }
    }

    Dialog {
        id: toolDialog
        width: 520
        height: 380
        modal: true
        anchors.centerIn: Overlay.overlay
        background: Rectangle {
            color: Theme.cardFill
            radius: 12
            border.color: Theme.glassBorder
            border.width: 1
        }
        contentItem: Column {
            spacing: 8
            ScrollView {
                width: parent.width
                height: 300
                clip: true
                TextArea {
                    id: toolResult
                    width: parent.width
                    color: Theme.text
                    font.pixelSize: 12
                    font.family: "Consolas"
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    background: null
                }
            }
            AppButton {
                text: "关闭"
                anchors.horizontalCenter: parent.horizontalCenter
                implicitWidth: 100
                onClicked: toolDialog.close()
            }
        }
    }

    ReportDialog {
        id: reportDialog
    }
}
