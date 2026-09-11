// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Components"

// Network dashboard: connection status first, then quick actions,
// then diagnostic tools with inline results.
Page {
    id: page
    padding: 0
    background: Rectangle { color: "transparent" }

    property string proxyStatus: "检测中..."
    property bool proxyBusy: false
    property bool proxyOnline: false

    // loading state per diagnostic tool (drives ResultPanel.busy)
    property bool nodeBusy: false
    property bool speedBusy: false
    property bool diagBusy: false

    // dashboard metrics
    property string latencyText: "--"
    property string speedText: "--"
    property string lastUpdateText: "--:--:--"
    property string lastLatency: "--"

    property color qualityColor: Theme.statusIdle
    property int qualityPercent: 0
    property string qualityDesc: "等待检测"

    function updateQuality() {
        // latency -> quality mapping
        var pct = 100
        var desc = "网络质量优秀"
        if (page.proxyOnline) {
            pct = 90
            desc = "梯子连接正常"
            if (page.lastLatency !== "--" && page.lastLatency !== "失败") {
                var ms = parseFloat(page.lastLatency)
                if (ms >= 0 && ms < 80) { pct = 100; desc = "网络质量优秀" }
                else if (ms < 150) { pct = 85; desc = "网络质量良好" }
                else if (ms < 300) { pct = 65; desc = "网络延迟偏高" }
                else { pct = 40; desc = "网络较差" }
            }
            page.qualityColor = Theme.ok
        } else {
            pct = 0
            desc = "未连接梯子"
            page.qualityColor = Theme.warn
        }
        page.qualityPercent = pct
        page.qualityDesc = desc
    }

    function refreshStatus() {
        proxyService.statusAsync()
    }

    function notify(msg) {
        appCore.showToast(msg)
        appCore.setStatus(msg)
    }

    function launchProxy(which) {
        page.proxyBusy = true
        appCore.setStatus("打开梯子中...")
        var ok = which === "Clash" ? proxyService.launchClash() : proxyService.launchV2ray()
        notify(ok ? "已打开 " + which : "打开失败")
        page.proxyBusy = false
        if (ok) statsService.record("proxy", which)
        page.refreshStatus()
    }

    Connections {
        target: proxyService
        function onStatusReady(status) {
            page.proxyStatus = status
            page.proxyOnline = status.indexOf("●") >= 0
            page.lastUpdateText = new Date().toTimeString().split(" ")[0]
            page.updateQuality()
        }
    }

    Component.onCompleted: {
        refreshStatus()
        statusTimer.start()
    }

    Timer {
        id: statusTimer
        interval: 3000
        repeat: true
        onTriggered: page.refreshStatus()
    }

    // fixed header + scrollable body (same layout as Settings page)
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.sp5
        spacing: Theme.sp4

        PageHeader {
            title: "网络工具"
            subtitle: "让我帮你检查一下网络状态"
        }

        ScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.vertical: AppScrollBar {}
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: scroll.availableWidth
                spacing: Theme.sp4

            // ================= 分区① 连接状态（Dashboard 视觉中心） =================
            SectionLabel { text: "连接状态"; hint: "每 3 秒自动刷新" }
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.sp5
                    anchors.rightMargin: Theme.sp5
                    anchors.topMargin: Theme.sp4
                    anchors.bottomMargin: Theme.sp4
                    spacing: Theme.sp3

                    // top row: health ring + status + quality badge
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.sp4

                        Rectangle {
                            width: 52; height: 52; radius: 26
                            color: "transparent"
                            border.color: qualityColor
                            border.width: 3
                            Behavior on border.color { ColorAnimation { duration: Theme.durMid } }
                            Text {
                                anchors.centerIn: parent
                                text: qualityPercent + "%"
                                color: Theme.text
                                font.pixelSize: Theme.fsSmall
                                font.bold: true
                                opacity: page.proxyBusy ? 0.4 : 1
                                Behavior on opacity { NumberAnimation { duration: Theme.durMid } }
                            }
                        }
                        Column {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                text: page.proxyStatus
                                color: Theme.text
                                font.pixelSize: Theme.fsTitle
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Text {
                                text: qualityDesc
                                color: Theme.textDim
                                font.pixelSize: Theme.fsSmall
                                elide: Text.ElideRight
                            }
                        }
                        StatusBadge {
                            state: page.proxyBusy ? "busy" : page.proxyOnline ? "ok" : "warn"
                            label: page.proxyBusy ? "连接中" : page.proxyOnline ? "已连接" : "未连接"
                        }
                    }

                    // bottom row: latency / download / last update metrics
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.sp4

                        // latency metric
                        Column {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                text: "延迟"
                                color: Theme.textDim
                                font.pixelSize: Theme.fsCaption
                            }
                            Text {
                                text: page.latencyText
                                color: Theme.text
                                font.pixelSize: Theme.fsDefault
                                font.bold: true
                            }
                        }
                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 26
                            color: Theme.glassBorder
                        }
                        // download metric
                        Column {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                text: "下载速度"
                                color: Theme.textDim
                                font.pixelSize: Theme.fsCaption
                            }
                            Text {
                                text: page.speedText
                                color: Theme.text
                                font.pixelSize: Theme.fsDefault
                                font.bold: true
                            }
                        }
                        Rectangle {
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 26
                            color: Theme.glassBorder
                        }
                        // last update
                        Column {
                            Layout.fillWidth: true
                            spacing: 1
                            Text {
                                text: "最近更新"
                                color: Theme.textDim
                                font.pixelSize: Theme.fsCaption
                            }
                            Text {
                                text: page.lastUpdateText
                                color: Theme.text
                                font.pixelSize: Theme.fsDefault
                                font.bold: true
                            }
                        }
                    }
                }
            }

            // ================= 分区② 快速操作 =================
            SectionLabel { text: "快速操作" }
            Card {
                Layout.fillWidth: true
                Layout.preferredHeight: 148
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp4
                    spacing: Theme.sp3

                    Text { text: "导入节点"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.sp3
                        TextField {
                            id: input
                            Layout.fillWidth: true
                            Layout.preferredHeight: 38
                            color: Theme.text
                            placeholderText: "粘贴 vmess / vless / trojan / ss 链接 或 订阅地址"
                            placeholderTextColor: Theme.textDim
                            background: Rectangle { color: Theme.inputBg; radius: Theme.rMd }
                        }
                        AppButton {
                            text: "导入到梯子"
                            variant: "secondary"
                            implicitWidth: 120
                            implicitHeight: 38
                            font.pixelSize: Theme.fsSmall
                            onClicked: {
                                if (input.text.trim().length === 0) {
                                    notify("请先粘贴配置链接或订阅地址")
                                    return
                                }
                                proxyService.copyToClipboard(input.text.trim())
                                notify("已复制到剪贴板，请在梯子中确认导入")
                            }
                        }
                        AppButton {
                            text: "订阅文件"
                            variant: "secondary"
                            implicitWidth: 100
                            implicitHeight: 38
                            font.pixelSize: Theme.fsSmall
                            onClicked: notify("请选择订阅文件（文件选择框预留）")
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.sp3
                        AppButton {
                            text: "打开梯子"
                            variant: "secondary"
                            Layout.fillWidth: true
                            implicitHeight: 38
                            enabled: !page.proxyBusy
                            onClicked: {
                                var clash = proxyService.hasClash()
                                var v2ray = proxyService.hasV2ray()
                                if (clash && v2ray) {
                                    // single shared island (routed via AppCore)
                                    appCore.showIsland("打开哪个梯子？", ["Clash Verge", "v2rayN"], "open_proxy")
                                } else if (clash) {
                                    launchProxy("Clash")
                                } else if (v2ray) {
                                    launchProxy("v2rayN")
                                } else {
                                    notify("未找到梯子程序")
                                }
                            }
                        }
                        AppButton {
                            text: "清空输入"
                            variant: "secondary"
                            Layout.fillWidth: true
                            implicitHeight: 38
                            onClicked: input.text = ""
                        }
                        AppButton {
                            text: "刷新状态"
                            variant: "secondary"
                            Layout.fillWidth: true
                            implicitHeight: 38
                            onClicked: page.refreshStatus()
                        }
                    }
                }
            }

            // ================= 分区③ 诊断工具 =================
            SectionLabel { text: "诊断工具" }
            GridLayout {
                Layout.leftMargin: Theme.sp1
                Layout.rightMargin: Theme.sp1
                Layout.fillWidth: true
                columns: 2
                columnSpacing: Theme.sp5
                rowSpacing: Theme.sp5

                // 节点测速 + 就近结果
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.sp2
                    Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 72
                        clickable: true
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.sp4
                            anchors.rightMargin: Theme.sp4
                            spacing: 2
                            Text { text: "节点测速"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "检测各站点延迟"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        onClicked: {
                            nodeResultText.text = ""
                            page.nodeBusy = true
                            notify("节点测速中...")
                            netService.nodeTestAsync()
                        }
                    }
                    ResultPanel {
                        Layout.fillWidth: true
                        title: "节点延迟"
                        emptyHint: "点击上方卡片开始测速"
                        busy: page.nodeBusy
                        text: nodeResultText.text
                    }
                }

                // 网络测速 + 就近结果
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: Theme.sp2
                    Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 72
                        clickable: true
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.sp4
                            anchors.rightMargin: Theme.sp4
                            spacing: 2
                            Text { text: "网络测速"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "测下载速度"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        onClicked: {
                            speedResultText.text = ""
                            page.speedBusy = true
                            notify("测速中...")
                            netService.speedTestAsync(true, 10 * 1024 * 1024)
                        }
                    }
                    ResultPanel {
                        Layout.fillWidth: true
                        title: "下载速度"
                        emptyHint: "点击上方卡片开始测速"
                        busy: page.speedBusy
                        text: speedResultText.text
                    }
                }

                // 故障自检 + 就近结果
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp2
                    Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 72
                        clickable: true
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.sp4
                            anchors.rightMargin: Theme.sp4
                            spacing: 2
                            Text { text: "故障自检"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "端口 / DNS / 连通性"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        onClicked: {
                            diagResultText.text = ""
                            page.diagBusy = true
                            notify("开始自检...")
                            netService.diagnoseAsync()
                        }
                    }
                    ResultPanel {
                        Layout.fillWidth: true
                        title: "自检报告"
                        emptyHint: "点击上方卡片开始自检"
                        busy: page.diagBusy
                        text: diagResultText.text
                    }
                }

                // 其他工具
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp2
                    Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 72
                        clickable: true
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.sp4
                            anchors.rightMargin: Theme.sp4
                            spacing: 2
                            Text { text: "下载梯子"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "打开 Clash Verge 下载页"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        onClicked: {
                            Qt.openUrlExternally("https://github.com/clash-verge-rev/clash-verge-rev/releases")
                            notify("已打开下载页面")
                        }
                    }
                    Card {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 72
                        clickable: true
                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.leftMargin: Theme.sp4
                            anchors.rightMargin: Theme.sp4
                            spacing: 2
                            Text { text: "遇到难题找 GPT"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                            Text { text: "自动开梯子并打开 ChatGPT"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                        }
                        onClicked: {
                            proxyService.launchAny()
                            Qt.openUrlExternally("https://chatgpt.com/")
                            notify("正在前往 ChatGPT...")
                        }
                    }
                }
            }

            Item { Layout.preferredHeight: Theme.sp6 }
            }
        }
    }

    // result text holders (hidden, driven by network service callback)
    Text { id: nodeResultText; visible: false }
    Text { id: speedResultText; visible: false }
    Text { id: diagResultText; visible: false }

    Connections {
        target: netService
        function onResult(text) {
            if (text.indexOf("google.com") >= 0) {
                nodeResultText.text = text
                page.nodeBusy = false
                appCore.showToast("节点测速完成")
                // update dashboard latency (first line usually has ms)
                page.lastLatency = "--"
                var m = text.match(/(\d+(?:\.\d+)?)\s*ms/i)
                if (m) page.lastLatency = m[1]
                page.latencyText = page.lastLatency + " ms"
                page.updateQuality()
            } else if (text.indexOf("=== 进程检测 ===") >= 0) {
                // self-diagnosis result shows inside the connection status panel
                diagResultText.text = text
                page.diagBusy = false
                appCore.showToast("自检完成")
            } else if (text.indexOf("Mbps") >= 0) {
                speedResultText.text = text
                page.speedBusy = false
                appCore.showToast("测速完成")
                var sm = text.match(/(\d+(?:\.\d+)?)\s*Mbps/i)
                if (sm) page.speedText = sm[1] + " Mbps"
            }
        }
    }
}
