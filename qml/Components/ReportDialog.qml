// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Components"

// Usage report dialog (ported from the original WinForms report):
// 5 tabs with bar charts drawn on Canvas.
Dialog {
    id: report
    width: 620
    height: 460
    modal: true
    anchors.centerIn: Overlay.overlay
    title: "使用报告"

    background: Rectangle {
        color: Theme.cardFill
        radius: 12
        border.color: Theme.glassBorder
        border.width: 1
    }

    property int tab: 0
    property var chartData: []

    function loadData() {
        var arr = []
        if (tab === 0) arr = statsService.overviewStats()
        else if (tab === 1) arr = statsService.categoryCounts()
        else if (tab === 2) arr = statsService.moodCounts()
        else if (tab === 3) arr = statsService.cleanDaily()
        else arr = statsService.proxyCounts()
        chartData = arr
        chartCanvas.requestPaint()
    }

    contentItem: Column {
        spacing: 8

        // tab buttons
        RowLayout {
            width: parent.width
            spacing: 4
            Repeater {
                model: ["总览", "抽卡", "心情", "清理", "梯子"]
                AppButton {
                    text: modelData
                    glassColor: report.tab === index ? Theme.glassHover : Theme.glass
                    glassHover: Theme.glassHover
                    glassPress: Theme.glassPress
                    Layout.fillWidth: true
                    implicitHeight: 30
                    onClicked: { report.tab = index; report.loadData() }
                }
            }
        }

        // bar chart area
        Canvas {
            id: chartCanvas
            width: parent.width
            height: 330
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.clearRect(0, 0, width, height)

                if (report.chartData.length === 0) {
                    ctx.fillStyle = Theme.textDim
                    ctx.font = "14px sans-serif"
                    ctx.textAlign = "center"
                    ctx.fillText("暂无数据", width / 2, height / 2)
                    return
                }

                var pad = 50
                var plotW = width - pad * 2
                var plotH = height - 70
                var maxV = 1
                for (var i = 0; i < report.chartData.length; i++)
                    if (report.chartData[i].value > maxV) maxV = report.chartData[i].value

                var n = report.chartData.length
                var barW = Math.min(50, plotW / n - 10)
                var gap = (plotW - barW * n) / (n + 1)

                // grid + labels
                ctx.strokeStyle = Theme.glassBorder
                ctx.fillStyle = Theme.textDim
                ctx.font = "11px sans-serif"
                ctx.textAlign = "right"
                for (var g = 0; g <= 4; g++) {
                    var gy = pad + plotH - (plotH * g / 4)
                    ctx.beginPath()
                    ctx.moveTo(pad, gy)
                    ctx.lineTo(width - pad, gy)
                    ctx.stroke()
                    ctx.fillText(Math.round(maxV * g / 4), pad - 6, gy + 4)
                }

                // bars
                for (var j = 0; j < n; j++) {
                    var v = report.chartData[j].value
                    var bh = plotH * (v / maxV)
                    var bx = pad + gap + j * (barW + gap)
                    var by = pad + plotH - bh

                    // bar body
                    ctx.fillStyle = Theme.accent
                    ctx.fillRect(bx, by, barW, bh)

                    // value above
                    ctx.fillStyle = Theme.text
                    ctx.textAlign = "center"
                    ctx.fillText(v, bx + barW / 2, by - 6)

                    // label below
                    ctx.fillStyle = Theme.textDim
                    ctx.fillText(report.chartData[j].label, bx + barW / 2, pad + plotH + 18)
                }
            }
        }
    }

    footer: AppButton {
        text: "关闭"
        anchors.horizontalCenter: parent.horizontalCenter
        implicitWidth: 100
        onClicked: report.close()
    }
}
