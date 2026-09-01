import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// AI profile feature bundle: the main profile dialog (overview + 6 detail
// pages) plus its memory viewer / memory input sub-dialogs.
// Host supplies aiAvatarSource and handles toast / profileSaved /
// avatarPickRequested so this file stays free of app-shell coupling.
// NOTE: root fills the window because DialogContainer derives its size and
// centering from its parent item.
Item {
    id: dlg

    anchors.fill: parent

    property string aiAvatarSource: ""
    signal toast(string message)
    signal profileSaved()
    signal avatarPickRequested()

    function openProfile() { aiProfileDialog.open() }

    // =====================================================================
    // AI PROFILE — single unified container (no nested dialogs)
    // page 0 = overview (header + category cards)
    // pages 1..6 = detail panels for each category (back button returns)
    // =====================================================================
    DialogContainer {
        id: aiProfileDialog
        dialogTitle: "AI 资料"
        dialogSubtitle: "角色、记忆与关系管理"
        dialogWidth: 600
        dialogHeight: 640

        // detail navigation state
        property int detailPage: 0   // 0=overview, 1..6=detail

        function showDetail(p) {
            aiProfileDialog.detailPage = p
            profileStack.currentIndex = p
            aiProfileDialog.dialogTitle = aiProfileDialog.detailTitle(p)
            aiProfileDialog.dialogSubtitle = aiProfileDialog.detailSubtitle(p)
        }
        function showOverview() {
            aiProfileDialog.detailPage = 0
            profileStack.currentIndex = 0
            aiProfileDialog.dialogTitle = "AI 资料"
            aiProfileDialog.dialogSubtitle = "角色、记忆与关系管理"
        }
        function detailTitle(p) {
            return ["AI 资料", "AI 人设", "共同经历", "与我的关系", "我的兴趣", "未完成话题", "使用统计"][p]
        }
        function detailSubtitle(p) {
            return ["角色、记忆与关系管理",
                    "角色设定和性格",
                    "重要事件记忆",
                    "亲密度和关系设置",
                    "兴趣偏好",
                    "聊天中断的话题",
                    "陪伴时间"][p]
        }

        StackLayout {
            id: profileStack
            anchors.fill: parent
            currentIndex: aiProfileDialog.detailPage

            // ---------------- page 0: overview ----------------
            Item {
                ScrollView {
                    id: profileScroll
                    anchors.fill: parent
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ColumnLayout {
                        width: profileScroll.availableWidth
                        anchors.margins: Theme.sp5
                        spacing: Theme.sp3

                    ProfileHeader {
                        Layout.fillWidth: true
                        avatarSource: dlg.aiAvatarSource
                        name: aiService.aiName()
                        status: "在线"
                        statusOnline: true
                        onAvatarClicked: dlg.avatarPickRequested()
                    }

                    // category cards
                    SettingCard {
                        Layout.fillWidth: true
                        iconText: "A"
                        title: "AI 人设"
                        description: "角色设定和性格"
                        onClicked: aiProfileDialog.showDetail(1)
                    }
                    SettingCard {
                        Layout.fillWidth: true
                        iconText: "E"
                        title: "共同经历"
                        description: "重要事件记忆"
                        onClicked: aiProfileDialog.showDetail(2)
                    }
                    SettingCard {
                        Layout.fillWidth: true
                        iconText: "R"
                        title: "与我的关系"
                        description: "亲密度和关系设置"
                        onClicked: aiProfileDialog.showDetail(3)
                    }
                    SettingCard {
                        Layout.fillWidth: true
                        iconText: "I"
                        title: "我的兴趣"
                        description: "兴趣偏好"
                        onClicked: aiProfileDialog.showDetail(4)
                    }
                    SettingCard {
                        Layout.fillWidth: true
                        iconText: "T"
                        title: "未完成话题"
                        description: "聊天中断的话题"
                        onClicked: aiProfileDialog.showDetail(5)
                    }
                    SettingCard {
                        Layout.fillWidth: true
                        iconText: "S"
                        title: "使用统计"
                        description: "陪伴时间"
                        onClicked: aiProfileDialog.showDetail(6)
                    }

                    Item { Layout.fillHeight: true }

                    // memory quick actions
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.sp3
                        AppButton {
                            text: "查看记忆"
                            variant: "secondary"
                            Layout.fillWidth: true
                            onClicked: memoryViewDialog.open()
                        }
                        AppButton {
                            text: "添加记忆"
                            variant: "secondary"
                            Layout.fillWidth: true
                            onClicked: {
                                memoryInput.text = ""
                                memoryInputDialog.open()
                            }
                        }
                        AppButton {
                            text: "清空记忆"
                            variant: "secondary"
                            Layout.fillWidth: true
                            borderColor: Qt.rgba(0.77,0.35,0.35,0.55)
                            onClicked: {
                                aiService.clearMemory()
                                dlg.toast("记忆已清空~")
                            }
                        }
                    }
                    }
                }
            }

            // ---------------- page 1: AI 人设 ----------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp5
                    spacing: Theme.sp3

                    AppButton {
                        text: "‹ 返回"
                        variant: "ghost"
                        implicitWidth: 80
                        onClicked: aiProfileDialog.showOverview()
                    }
                    Text { text: "AI 名字"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    ThemedTextField {
                        id: profileAiName
                        placeholderText: "AI 的名字"
                        text: aiService.aiName()
                        onAccepted: saveProfileBtn.clicked()
                    }
                    Text { text: "AI 人设"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.inputBg
                        radius: Theme.rMd
                        clip: true
                        border.width: 1
                        // focus ring follows the text area, glass-aware
                        border.color: profileAiPersonality.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
                        Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: Theme.sp2
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            TextArea {
                                id: profileAiPersonality
                                width: parent.width - Theme.sp2
                                height: Math.max(parent.height, implicitHeight)
                                color: Theme.text
                                text: aiService.aiPersonality()
                                placeholderText: "描述 AI 的性格、语气与陪伴风格…"
                                placeholderTextColor: Theme.textDim
                                wrapMode: TextEdit.Wrap
                                background: null
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: profileAiPersonality.text.length + " 字"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fsCaption
                        horizontalAlignment: Text.AlignRight
                    }
                    AppButton {
                        id: saveProfileBtn
                        text: "保存 AI 人设"
                        variant: "primary"
                        Layout.fillWidth: true
                        onClicked: {
                            aiService.setAiName(profileAiName.text)
                            aiService.setAiPersonality(profileAiPersonality.text)
                            dlg.profileSaved()
                            dlg.toast("AI 资料已保存~")
                        }
                    }
                }
            }

            // ---------------- page 2: 共同经历 (read-only) ----------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp5
                    spacing: Theme.sp3

                    AppButton {
                        text: "‹ 返回"
                        variant: "ghost"
                        implicitWidth: 80
                        onClicked: aiProfileDialog.showOverview()
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.inputBg
                        radius: Theme.rMd
                        clip: true
                        border.width: 1
                        border.color: Theme.inputBorder
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: Theme.sp2
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            TextArea {
                                id: eventsText
                                width: parent.width
                                color: Theme.text
                                font.pixelSize: Theme.fsDefault
                                readOnly: true
                                wrapMode: TextEdit.Wrap
                                background: null
                                text: aiService.eventMemoryText(50)
                            }
                        }
                    }
                }
            }

            // ---------------- page 3: 与我的关系 ----------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp5
                    spacing: Theme.sp4

                    AppButton {
                        text: "‹ 返回"
                        variant: "ghost"
                        implicitWidth: 80
                        onClicked: aiProfileDialog.showOverview()
                    }
                    Card {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.sp4
                            spacing: Theme.sp3

                            Text { text: "亲密度"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                            RowLayout {
                                Layout.fillWidth: true
                                DarkSlider {
                                    id: relIntimacy
                                    Layout.fillWidth: true
                                    from: 0; to: 100; stepSize: 5
                                    Component.onCompleted: value = aiService.relationshipIntimacy()
                                    onValueChanged: aiService.setRelationship(value, aiService.relationshipTrust())
                                }
                                Text { text: relIntimacy.value; color: Theme.textDim; font.pixelSize: Theme.fsCaption }
                            }
                            Text { text: "信任度"; color: Theme.textDim; font.pixelSize: Theme.fsSmall }
                            RowLayout {
                                Layout.fillWidth: true
                                DarkSlider {
                                    id: relTrust
                                    Layout.fillWidth: true
                                    from: 0; to: 100; stepSize: 5
                                    Component.onCompleted: value = aiService.relationshipTrust()
                                    onValueChanged: aiService.setRelationship(aiService.relationshipIntimacy(), value)
                                }
                                Text { text: relTrust.value; color: Theme.textDim; font.pixelSize: Theme.fsCaption }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: {
                                    var itm = relIntimacy.value
                                    var tr = relTrust.value
                                    var level = "刚认识"
                                    if (itm >= 80) level = "形影不离"
                                    else if (itm >= 60) level = "很亲近"
                                    else if (itm >= 40) level = "好朋友"
                                    else if (itm >= 20) level = "逐渐熟悉"
                                    return "亲密度 " + itm + "，信任度 " + tr + "：" + level + " 的关系。"
                                }
                                color: Theme.textDim
                                font.pixelSize: Theme.fsSmall
                                wrapMode: Text.Wrap
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }
                }
            }

            // ---------------- page 4: 我的兴趣 ----------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp5
                    spacing: Theme.sp3

                    AppButton {
                        text: "‹ 返回"
                        variant: "ghost"
                        implicitWidth: 80
                        onClicked: aiProfileDialog.showOverview()
                    }
                    ListView {
                        id: interestsListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: Theme.sp2
                        model: aiService.interestList()
                        delegate: RowLayout {
                            width: interestsListView.width
                            spacing: Theme.sp3
                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                color: Theme.text
                                font.pixelSize: Theme.fsDefault
                                elide: Text.ElideRight
                            }
                            AppButton {
                                text: "删除"
                                variant: "ghost"
                                implicitHeight: 30
                                onClicked: {
                                    aiService.removeInterest(modelData)
                                    interestsListView.model = aiService.interestList()
                                }
                            }
                        }
                        // empty state: guide first-time users
                        Text {
                            anchors.centerIn: parent
                            visible: interestsListView.count === 0
                            text: "还没有添加兴趣\n和 AI 聊聊你喜欢的事，或在下方添加"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fsSmall
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.sp3
                        ThemedTextField {
                            id: interestInput
                            placeholderText: "添加一个兴趣（如 Minecraft）"
                            onAccepted: addInterestBtn.clicked()
                        }
                        AppButton {
                            id: addInterestBtn
                            text: "添加"
                            variant: "secondary"
                            implicitHeight: 38
                            onClicked: {
                                aiService.addInterest(interestInput.text)
                                interestInput.text = ""
                                interestsListView.model = aiService.interestList()
                            }
                        }
                    }
                }
            }

            // ---------------- page 5: 未完成话题 ----------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp5
                    spacing: Theme.sp3

                    AppButton {
                        text: "‹ 返回"
                        variant: "ghost"
                        implicitWidth: 80
                        onClicked: aiProfileDialog.showOverview()
                    }
                    ListView {
                        id: topicsListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: Theme.sp2
                        model: aiService.topicList()
                        delegate: RowLayout {
                            width: topicsListView.width
                            spacing: Theme.sp3
                            Text {
                                Layout.fillWidth: true
                                text: modelData
                                color: Theme.text
                                font.pixelSize: Theme.fsDefault
                                wrapMode: Text.Wrap
                            }
                            AppButton {
                                text: "删除"
                                variant: "ghost"
                                implicitHeight: 30
                                onClicked: {
                                    aiService.removeTopic(modelData)
                                    topicsListView.model = aiService.topicList()
                                }
                            }
                        }
                        // empty state: guide first-time users
                        Text {
                            anchors.centerIn: parent
                            visible: topicsListView.count === 0
                            text: "没有未完成的话题\n聊到一半被打断的话题会自动出现在这里"
                            color: Theme.textMuted
                            font.pixelSize: Theme.fsSmall
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.sp3
                        ThemedTextField {
                            id: topicInput
                            placeholderText: "添加一个聊到一半的话题"
                            onAccepted: addTopicBtn.clicked()
                        }
                        AppButton {
                            id: addTopicBtn
                            text: "添加"
                            variant: "secondary"
                            implicitHeight: 38
                            onClicked: {
                                aiService.addTopic(topicInput.text)
                                topicInput.text = ""
                                topicsListView.model = aiService.topicList()
                            }
                        }
                    }
                }
            }

            // ---------------- page 6: 使用统计 ----------------
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp5
                    spacing: Theme.sp3

                    AppButton {
                        text: "‹ 返回"
                        variant: "ghost"
                        implicitWidth: 80
                        onClicked: aiProfileDialog.showOverview()
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: Theme.inputBg
                        radius: Theme.rMd
                        clip: true
                        border.width: 1
                        border.color: Theme.inputBorder
                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: Theme.sp2
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            TextArea {
                                id: usageTextArea
                                width: parent.width
                                color: Theme.text
                                font.pixelSize: Theme.fsDefault
                                readOnly: true
                                wrapMode: TextEdit.Wrap
                                background: null
                                text: aiService.usageText() + "\n\n" + aiService.uptimeText()
                            }
                        }
                    }
                }
            }
        }
    }

    // memory viewer dialog: shows the AI's full memory report
    DialogContainer {
        id: memoryViewDialog
        dialogTitle: "查看记忆"
        dialogSubtitle: "AI 记住的关于你的一切"
        dialogWidth: 560
        dialogHeight: 520
        property bool editing: false
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.sp4
            spacing: Theme.sp3

            // edit-mode indicator banner
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 28
                radius: Theme.rMd
                color: Theme.glassMode ? Qt.rgba(0.95,0.75,0.3,0.15) : Qt.rgba(0.95,0.75,0.3,0.12)
                visible: memoryViewDialog.editing
                Text {
                    anchors.centerIn: parent
                    text: "⚠ 编辑模式 — 修改后请点「保存修改」"
                    color: Theme.glassMode ? "#E6B84C" : "#B8860B"
                    font.pixelSize: Theme.fsCaption
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.inputBg
                radius: Theme.rMd
                clip: true
                border.width: 1
                border.color: memoryViewDialog.editing ? Theme.inputFocusBorder : Theme.inputBorder
                Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: Theme.sp2
                    clip: true
                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                    TextArea {
                        id: memoryReportText
                        width: parent.width
                        color: Theme.text
                        font.pixelSize: Theme.fsSmall
                        readOnly: true
                        wrapMode: TextEdit.Wrap
                        background: null
                        text: aiService.memoryDetail()
                        onReadOnlyChanged: { if (!readOnly) text = aiService.memoryRaw() }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.sp3
                AppButton {
                    text: memoryViewDialog.editing ? "取消编辑" : "编辑原始记忆"
                    variant: "secondary"
                    Layout.fillWidth: true
                    onClicked: {
                        if (memoryViewDialog.editing) {
                            memoryReportText.readOnly = true
                            memoryReportText.text = aiService.memoryDetail()
                        } else {
                            memoryReportText.readOnly = false
                            memoryReportText.text = aiService.memoryRaw()
                        }
                        memoryViewDialog.editing = !memoryViewDialog.editing
                    }
                }
                AppButton {
                    text: "保存修改"
                    variant: "primary"
                    Layout.fillWidth: true
                    enabled: memoryViewDialog.editing
                    onClicked: {
                        aiService.setMemoryRaw(memoryReportText.text)
                        memoryReportText.readOnly = true
                        memoryReportText.text = aiService.memoryDetail()
                        memoryViewDialog.editing = false
                        dlg.toast("记忆已保存~")
                    }
                }
            }
        }
    }

    // small add-memory dialog (kept minimal on purpose)
    DialogContainer {
        id: memoryInputDialog
        dialogTitle: "添加记忆"
        dialogSubtitle: "告诉 AI 一件值得记住的事"
        dialogWidth: 460
        dialogHeight: 300
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Theme.sp5
            spacing: Theme.sp3

            TextArea {
                id: memoryInput
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Theme.text
                placeholderText: "比如：我喜欢喝奶茶，讨厌下雨天…"
                placeholderTextColor: Theme.textDim
                wrapMode: TextEdit.Wrap
                background: Rectangle {
                    radius: Theme.rMd
                    color: Theme.inputBg
                    border.width: 1
                    border.color: memoryInput.activeFocus ? Theme.inputFocusBorder : Theme.inputBorder
                    Behavior on border.color { ColorAnimation { duration: Theme.durFast } }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Theme.sp3
                AppButton {
                    text: "记住"
                    variant: "primary"
                    Layout.fillWidth: true
                    onClicked: {
                        aiService.addMemoryNote(memoryInput.text)
                        dlg.toast("记住了~")
                        memoryInputDialog.close()
                    }
                }
                AppButton {
                    text: "取消"
                    variant: "secondary"
                    Layout.fillWidth: true
                    onClicked: memoryInputDialog.close()
                }
            }
        }
    }
}
