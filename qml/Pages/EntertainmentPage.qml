import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Components"

// Entertainment: companion-focused — today's fortune, fun tools, mood journal.
Page {
    id: root
    padding: 0
    background: Rectangle { color: "transparent" }

    property string result: ""
    property bool prankOn: false

    function randomInt(min, max) {
        return Math.floor(Math.random() * (max - min + 1)) + min
    }

    function luckyValue() {
        var d = new Date()
        var seed = d.getFullYear() * 10000 + (d.getMonth() + 1) * 100 + d.getDate()
        var x = Math.sin(seed * 999) * 10000
        return Math.floor((x - Math.floor(x)) * 100) + 1
    }

    // fixed header + scrollable body (same layout as Settings page)
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.sp5
        spacing: Theme.sp4

        PageHeader {
            title: "娱乐 · UwU"
            subtitle: "陪你看今天的运势，记下小心情"
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

            // ================= 分区① 今日状态 =================
            SectionLabel { text: "今日状态" }
            GridLayout {
                Layout.leftMargin: Theme.sp1
                Layout.rightMargin: Theme.sp1
                Layout.fillWidth: true
                columns: 3
                columnSpacing: Theme.sp5
                rowSpacing: Theme.sp5

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "今日幸运值"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "看看今天的欧气"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                    onClicked: {
                        var v = root.luckyValue()
                        var verdict = ""
                        if (v >= 90) verdict = "欧气爆棚!! 去买彩票吧 Ovo"
                        else if (v >= 70) verdict = "今天运气不错~"
                        else if (v >= 40) verdict = "平平淡淡才是真"
                        else if (v >= 20) verdict = "有点小霉运，注意脚下 QwQ"
                        else verdict = "低调点……别立flag"
                        root.result = "今日幸运值：" + v + "/100\n" + verdict
                    }
                }
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "今日宜忌"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "泉此方钦定"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                    onClicked: {
                        var yi = ["出行","会友","交易","祈福","求财","学习","运动","表白","搬家","装修"]
                        var ji = ["熬夜","借贷","争吵","远行","动土","发誓","冒险","拖延","暴饮暴食","冲动消费"]
                        root.result = "今日宜：" + yi[root.randomInt(0, yi.length-1)] + "\n今日忌：" + ji[root.randomInt(0, ji.length-1)]
                    }
                }
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "抽签"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "一签定吉凶"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                    onClicked: {
                        var signs = ["大吉","中吉","小吉","吉","末吉","凶","大凶"]
                        var tips = ["心想事成 Ovo","运气不错~","小有福气","平平顺顺","稍安勿躁","宜低调 QwQ","宜躺平"]
                        var i = root.randomInt(0, signs.length-1)
                        root.result = "抽签结果：" + signs[i] + "\n" + tips[i]
                    }
                }
            }

            // 今日状态结果（就近显示）
            ResultPanel {
                Layout.fillWidth: true
                title: "今日结果"
                emptyHint: "点上面的卡片，看看今天运势"
                text: root.result
            }

            // ================= 分区② 趣味功能 =================
            SectionLabel { text: "趣味功能" }
            GridLayout {
                Layout.leftMargin: Theme.sp1
                Layout.rightMargin: Theme.sp1
                Layout.fillWidth: true
                columns: 3
                columnSpacing: Theme.sp5
                rowSpacing: Theme.sp5

                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "抽卡模拟器"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "SSR 保佑 Ovo"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                    onClicked: {
                        var roll = root.randomInt(1, 1000)
                        var rarity = ""
                        if (roll <= 6) rarity = "SSR ★★★★★"
                        else if (roll <= 46) rarity = "SR ★★★★"
                        else if (roll <= 196) rarity = "R ★★★"
                        else rarity = "N ★★"
                        root.result = "抽卡结果：" + rarity
                        statsService.record("gacha", rarity)
                    }
                }
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "整蛊模式"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text {
                            text: root.prankOn ? "运行中，随机弹「该休息了」" : "随机弹「该休息了」"
                            color: root.prankOn ? Theme.ok : Theme.textDim
                            font.pixelSize: Theme.fsSmall
                        }
                    }
                    onClicked: {
                        root.prankOn = !root.prankOn
                        if (root.prankOn) prankTimer.start()
                        else prankTimer.stop()
                        appCore.showToast(root.prankOn ? "整蛊模式已开启~" : "整蛊模式已关闭~")
                    }
                }
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    enabled: false
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "播放音乐"; color: Theme.textDim; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "即将推出"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                }
            }

            // ================= 分区③ 心情记录 =================
            SectionLabel { text: "心情记录" }
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
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "记录今天的心情"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "选一个心情记下来"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                    onClicked: moodDialog.open()
                }
                Card {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 84
                    clickable: true
                    Column {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: Theme.sp4
                        anchors.rightMargin: Theme.sp4
                        spacing: 2
                        Text { text: "心情历史"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Text { text: "回看最近的心情"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    }
                    onClicked: appCore.showToast(moodService.history())
                }
            }

            Item { Layout.preferredHeight: Theme.sp6 }
            }
        }
    }

    // ================= mood dialog (kept as-is) =================
    Dialog {
        id: moodDialog
        width: 380
        modal: true
        padding: 16
        anchors.centerIn: Overlay.overlay
        background: Rectangle {
            color: Theme.cardFill
            radius: 16
            border.color: Qt.rgba(255/255,180/255,220/255,0.25)
            border.width: 1
        }
        header: Item {
            height: 40
            Text {
                anchors.centerIn: parent
                text: "今天的心情是……？"
                color: Theme.text
                font.pixelSize: Theme.fsTitle
                font.bold: true
            }
            AppButton {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 28; height: 28
                text: "✕"
                implicitHeight: 28
                glassColor: "transparent"
                glassHover: Theme.glassHover
                glassPress: Theme.glassPress
                font.pixelSize: 14
                onClicked: moodDialog.close()
            }
        }
        contentItem: ScrollView {
            width: parent.width
            height: 320
            clip: true
            Column {
                width: parent.width
                spacing: 8
                Repeater {
                    model: moodService.moodOptions()
                    delegate: Rectangle {
                        width: parent.width
                        height: 44
                        radius: 22
                        color: mouse.hovered ? Qt.rgba(255/255,180/255,210/255,0.25) : Qt.rgba(255/255,180/255,210/255,0.10)
                        border.color: Qt.rgba(255/255, 200/255, 225/255, 0.25)
                        border.width: 1
                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            color: Theme.text
                            font.pixelSize: Theme.fsDefault
                            width: parent.width - 24
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.Wrap
                        }
                        MouseArea {
                            id: mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                moodService.recordMood(modelData)
                                statsService.record("mood", modelData)
                                root.result = "心情已记下~\n" + modelData
                                moodDialog.close()
                                appCore.showToast("心情已记下~ " + modelData)
                            }
                        }
                    }
                }
            }
        }
    }

    Timer {
        id: prankTimer
        interval: 60000
        repeat: true
        onTriggered: {
            if (root.prankOn) appCore.showToast("泉此方提示你该休息了~")
        }
    }
}
