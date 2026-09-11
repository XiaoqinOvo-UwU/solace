// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.LocalStorage
import "../Components"

// Full-screen chat page (QQ/WeChat style), opened from the sidebar AI card.
// Conversation is persisted per contact (SQLite) — reopening keeps the same chat.
// INDEPENDENT chat overlay: owns its own near-opaque background so business
// pages never show through. The wallpaper is only faintly felt behind it.
Rectangle {
    id: chatPage
    color: Theme.chatBg
    anchors.fill: parent

    // enter: slide in from LEFT with a bouncy OutBack; exit: slide out to RIGHT
    opacity: 0
    x: -width

    // ---- persistent per-contact chat (same conversation every time) ----
    function chatDb() {
        var db = LocalStorage.openDatabaseSync("XiaoQinChat", "1.0", "chat history", 8*1024*1024)
        return db
    }

    // WeChat-style time text: same day -> 下午 3:45 ; yesterday -> 昨天 23:10 ; older -> 8月12日 09:05
    function fmtTime(ts) {
        var d = new Date(ts)
        var now = new Date()
        var pad = function(n) { return (n < 10 ? "0" : "") + n }
        var h12 = d.getHours() % 12
        if (h12 === 0) h12 = 12
        var hm = (d.getHours() < 12 ? "上午 " : "下午 ") + h12 + ":" + pad(d.getMinutes())
        var dayStart = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime()
        var yestStart = dayStart - 24 * 3600 * 1000
        if (d.getTime() >= dayStart) return hm
        if (d.getTime() >= yestStart) return "昨天 " + hm
        return (d.getMonth() + 1) + "月" + d.getDate() + "日 " + hm
    }

    // 10 minutes = gap threshold for a time separator
    property int gapThresholdMs: 10 * 60 * 1000

    // reset the send/reply state machine. loadChat() reloads the conversation
    // for a contact; without this the in-flight flags (aiBusy/replyBusy) and
    // the timers leak across a contact switch, leaving the header stuck on
    // "正在输入" forever and silently swallowing every later message.
    function resetSendState() {
        mergeTimer.stop()
        readTimer.stop()
        typingTimer.stop()
        replyTimer.stop()
        chatPage.sendBuffer = []
        chatPage.replyQueue = []
        chatPage.pendingMerged = ""
        chatPage.pendingSendText = ""
        chatPage.pendingReadRows = []
        chatPage.pendingReply = ""
        chatPage.typingRow = -1
        chatPage.replyBusy = false
        chatPage.aiBusy = false
        chatPage.setHeaderStatus("在线")
    }

    function loadChat(contactId) {
        var db = chatDb()
        db.transaction(function(tx) {
            tx.executeSql("CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, contact TEXT, isAi INTEGER, msg TEXT, ts INTEGER)")
            try { tx.executeSql("ALTER TABLE messages ADD COLUMN ts INTEGER") } catch (e) { }
            var rs = tx.executeSql("SELECT isAi, msg, ts FROM messages WHERE contact=? ORDER BY id", [contactId])
            msgModel.clear()
            var hist = []
            var prevTs = -1
            for (var i = 0; i < rs.rows.length; i++) {
                var isAi = rs.rows.item(i).isAi === 1
                var msg = rs.rows.item(i).msg
                var ts = rs.rows.item(i).ts
                var timeLabel = ""
                if (ts > 0 && prevTs > 0 && (ts - prevTs) > chatPage.gapThresholdMs)
                    timeLabel = fmtTime(ts)
                if (ts > 0) prevTs = ts
                msgModel.append({ "isAi": isAi, "msg": msg, "timeLabel": timeLabel, "grouped": false, "ts": ts, "receipt": "" })
                hist.push((isAi ? aiService.aiName() : "用户") + ": " + msg)
            }
            chatPage.lastMsgTs = prevTs
            chatPage.resetSendState()
            // seed AI context with this conversation so it can see past messages
            aiService.setChatHistory(hist.join("\n"))
        })
        // if this contact has no history yet, show a greeting bubble
        if (msgModel.count === 0)
            msgModel.append({ "isAi": true, "msg": "你好，我是" + aiService.aiName() + "。", "timeLabel": "", "grouped": false, "ts": Date.now(), "receipt": "" })
        msgView.stick = true
        Qt.callLater(function() { if (msgView) msgView.pinBottom() })
    }

    function saveMsg(contactId, isAi, msg, ts) {
        var db = chatDb()
        db.transaction(function(tx) {
            tx.executeSql("CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, contact TEXT, isAi INTEGER, msg TEXT, ts INTEGER)")
            try { tx.executeSql("ALTER TABLE messages ADD COLUMN ts INTEGER") } catch (e) { }
            tx.executeSql("INSERT INTO messages (contact, isAi, msg, ts) VALUES (?,?,?,?)", [contactId, isAi ? 1 : 0, msg, ts || Date.now()])
            // keep history bounded (last 400)
            tx.executeSql("DELETE FROM messages WHERE id NOT IN (SELECT id FROM messages WHERE contact=? ORDER BY id DESC LIMIT 400)", [contactId])
        })
        chatPage.messageSaved(contactId, isAi, msg)
    }

    signal messageSaved(string contactId, bool isAi, string msg)

    property string currentContactId: ""

    // switch to a contact: load its conversation (no reset)
    function openContact(id) {
        currentContactId = id
        loadChat(id)
    }

    onVisibleChanged: {
        if (visible) {
            x = -width
            opacity = 0
            openAnim.restart()
            // load the current contact's history whenever the page shows
            var cid = contactService.currentId()
            if (currentContactId !== cid) {
                currentContactId = cid
                loadChat(cid)
            }
            chatPage.refreshApiStatus()
        } else {
            // reset so the next open always starts from the left
            x = -width
            opacity = 0
        }
    }
    property string lastGreetedId: ""

    // open animation (explicit object — no Behavior races)
    ParallelAnimation {
        id: openAnim
        NumberAnimation { target: chatPage; property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
        NumberAnimation { target: chatPage; property: "x"; from: -chatPage.width; to: 0; duration: 200; easing.type: Easing.OutCubic }
    }

    // closing animation: slide right, then notify parent to hide
    function closeWithAnim() {
        closeAnim.restart()
    }
    ParallelAnimation {
        id: closeAnim
        NumberAnimation { target: chatPage; property: "opacity"; to: 0; duration: 160; easing.type: Easing.InCubic }
        NumberAnimation { target: chatPage; property: "x"; to: chatPage.width; duration: 200; easing.type: Easing.InCubic }
        onFinished: chatPage.closeFinished()
    }

    signal backRequested()
    signal closeFinished()
    signal aiProfileRequested()

    // return the last N messages as "角色: 内容" lines (newest last)
    function recentMessages(n) {
        var out = []
        var start = Math.max(0, msgModel.count - n)
        for (var i = start; i < msgModel.count; i++) {
            var m = msgModel.get(i)
            if (m.isAi && m.msg === "...") continue   // typing placeholder, not a real message
            var who = m.isAi ? aiService.aiName() : "用户"
            out.push(who + ": " + m.msg)
        }
        return out
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // header: back button + AI info
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            color: Theme.chatPanelBg
            border.color: Theme.glassBorder
            border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 10
                spacing: 8

                AppButton {
                    text: "‹"
                    implicitWidth: 32
                    implicitHeight: 32
                    font.pixelSize: 18
                    glassColor: Theme.glass
                    glassHover: Theme.glassHover
                    glassPress: Theme.glassPress
                    onClicked: chatPage.backRequested()
                }

                // AI avatar (image or char) — click opens the AI profile dialog
                Rectangle {
                    width: 40; height: 40
                    radius: 20
                    color: Theme.accent
                    clip: true
                    scale: aiAvatarBtn.pressed ? 0.92 : 1.0
                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                    Image {
                        anchors.fill: parent
                        visible: chatPage.aiAvatarSource.length > 0
                        source: chatPage.aiAvatarSource
                        fillMode: Image.PreserveAspectCrop
                    }
                    Text {
                        anchors.centerIn: parent
                        text: chatPage.aiAvatarSource.length > 0 ? "" : (aiService.aiName().length > 0 ? aiService.aiName().charAt(0) : "A")
                        color: "white"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    MouseArea {
                        id: aiAvatarBtn
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: chatPage.aiProfileRequested()
                    }
                }

                Column {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        text: aiService.aiName()
                        color: Theme.text
                        font.pixelSize: 15
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                    }
                    // presence: colored dot + status (dot turns amber while typing)
                    Row {
                        spacing: 5
                        Rectangle {
                            id: statusDot
                            width: 7; height: 7; radius: 3.5
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.ok
                            Behavior on color { ColorAnimation { duration: Theme.durMid; easing.type: Easing.OutCubic } }
                        }
                        Text {
                            id: headerStatus
                            text: "在线"
                            color: Theme.ok
                            font.pixelSize: Theme.fsCaption
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // emotion badge: single character glyph + accent dot (project rule:
            // no emoji icons). Pops over the AI avatar, never touches name/status.
            Rectangle {
                id: emotionBadge
                width: 26; height: 26
                radius: 13
                color: Theme.cardFill
                border.color: Theme.glassBorder
                border.width: 1
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.leftMargin: 52
                visible: emotionEmoji.length > 0
                opacity: 0
                scale: 0.4
                Rectangle {
                    width: 5; height: 5; radius: 2.5
                    anchors.top: parent.top
                    anchors.topMargin: 3
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: emotionColor
                }
                Text {
                    id: emotionText
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 2
                    text: emotionEmoji
                    color: emotionColor
                    font.pixelSize: 13
                    font.bold: true
                }
                ParallelAnimation {
                    id: emotionPop
                    NumberAnimation { target: emotionBadge; property: "opacity"; from: 0; to: 1; duration: 180; easing.type: Easing.OutBack }
                    NumberAnimation { target: emotionBadge; property: "scale"; from: 0.4; to: 1.0; duration: 220; easing.type: Easing.OutBack }
                }
                SequentialAnimation {
                    id: emotionFade
                    PauseAnimation { duration: 2400 }
                    ParallelAnimation {
                        NumberAnimation { target: emotionBadge; property: "opacity"; to: 0; duration: 300; easing.type: Easing.InCubic }
                        NumberAnimation { target: emotionBadge; property: "scale"; to: 0.6; duration: 300; easing.type: Easing.InCubic }
                    }
                }
            }
        }

        // message list
        ListView {
            id: msgView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 8
            model: msgModel
            // ---- bottom-following -------------------------------------------
            // stick=true means "keep following new content". It flips false the
            // instant the user scrolls UP (direction-based, not just distance),
            // so new messages auto-scroll only while parked at the end, and a
            // scroll-up is never yanked back. We pin explicitly at real
            // content-change sites (new message / reply reveal) via
            // followBottom() — never on contentHeightChanged (delegate churn
            // during a scroll made that oscillate).
            property bool stick: true
            property real _lastY: 0
            readonly property real bottomEpsilon: 24

            function recomputeStick() {
                var dist = contentHeight - contentY - height
                if (contentHeight <= height + bottomEpsilon || dist <= bottomEpsilon)
                    stick = true
                else if (contentY < _lastY - 2)   // moved up -> user is browsing
                    stick = false
                _lastY = contentY
            }

            // jump to the newest message and resume following
            function pinBottom() { stick = true; positionViewAtEnd() }

            // follow new content while the user is at the end; pins now AND after
            // the layout pass so a wrapped-text reveal is covered too.
            function followBottom() {
                if (!stick) return
                positionViewAtEnd()
                Qt.callLater(function() { if (msgView.stick) msgView.positionViewAtEnd() })
            }

            onContentYChanged: recomputeStick()
            onMovementEnded: recomputeStick()
            onCountChanged: followBottom()
            delegate: Item {
                id: delegateRoot
                width: msgView.width
                readonly property bool showReceipt: !model.isAi && model.receipt === "已读"
                height: Math.max(44, bubbleRow.height)
                       + (model.timeLabel.length > 0 ? 30 : (model.grouped ? 4 : 14))

                // centered WeChat-style time separator for long gaps
                Text {
                    visible: model.timeLabel.length > 0
                    text: model.timeLabel
                    color: Theme.textDim
                    font.pixelSize: 10
                    opacity: 0.7
                    anchors.top: parent.top
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // TG-style message enter animation: slide up + fade in + subtle pop
                transform: Translate { id: msgTrans; y: 16 }
                Component.onCompleted: {
                    delegateRoot.opacity = 0
                    msgTrans.y = 16
                    animIn.start()
                }
                SequentialAnimation {
                    id: animIn
                    NumberAnimation { target: delegateRoot; property: "opacity"; from: 0; to: 1; duration: 200; easing.type: Easing.OutCubic }
                    NumberAnimation { target: msgTrans; property: "y"; from: 16; to: 0; duration: 240; easing.type: Easing.OutCubic }
                }

                Item {
                    id: bubbleRow
                    width: msgView.width
                    height: bubble.height
                    anchors.top: parent.top
                    anchors.topMargin: model.timeLabel.length > 0 ? 26
                                     : (model.grouped ? 0 : 8)

                    // avatars: AI left, user right (36px — readable next to bubbles)
                    Avatar {
                        id: aiAv
                        size: 36
                        source: chatPage.aiAvatarSource
                        charText: aiService.aiName().length > 0 ? aiService.aiName().charAt(0) : "A"
                        anchors.left: parent.left
                        anchors.leftMargin: 12
                        anchors.top: parent.top
                        // consecutive AI messages (multi-bubble reply) hide the
                        // avatar — only the first of the group keeps it, exactly
                        // like the user side
                        visible: model.isAi && !model.grouped
                    }
                    Avatar {
                        id: userAv
                        size: 36
                        source: chatPage.userAvatarSource
                        charText: aiService.userName().length > 0 ? aiService.userName().charAt(0) : "我"
                        anchors.right: parent.right
                        anchors.rightMargin: 12
                        anchors.top: parent.top
                        // consecutive user messages (batch) hide the avatar —
                        // only the first of the group keeps it (WeChat-style)
                        visible: !model.isAi && !model.grouped
                    }

                    MessageBubble {
                        id: bubble
                        isAi: model.isAi
                        text: model.msg
                        typing: model.msg === "..."     // animated three-dot reply state

                        // invisible center line: each side shares half the width,
                        // text wraps automatically once it would cross the line.
                        // The floor must stay BELOW the real half-width — a larger
                        // floor caps short messages early AND makes bubbles cross
                        // the center in narrow windows.
                        readonly property real centerLimit: Math.max(120, msgView.width / 2 - 56)
                        // +4 safety buffer: prevents exact-fit text (especially
                        // messages containing a space) from wrapping a glyph over
                        // to a second line.
                        width: model.msg === "..."
                                   ? 64
                                   : Math.min(bubble.contentWidth + 28 + 4, bubble.centerLimit)

                        anchors.left: model.isAi ? aiAv.right : undefined
                        anchors.leftMargin: model.isAi ? 8 : 0
                        anchors.right: model.isAi ? undefined : userAv.left
                        anchors.rightMargin: model.isAi ? 0 : 8
                        anchors.top: parent.top

                        // reveal pop when the typing placeholder turns into text
                        property string prevText: ""
                        onTextChanged: {
                            if (text !== "..." && text !== prevText) {
                                prevText = text
                                bubblePop.start()
                            }
                        }
                        transform: Scale { id: bubbleScale; xScale: 1; yScale: 1 }
                        ParallelAnimation {
                            id: bubblePop
                            NumberAnimation { target: bubbleScale; property: "xScale"; from: 0.97; to: 1.0; duration: 220; easing.type: Easing.OutBack }
                            NumberAnimation { target: bubbleScale; property: "yScale"; from: 0.97; to: 1.0; duration: 220; easing.type: Easing.OutBack }
                        }

                    }
                    // NOTE: no heightChanged pin here — a pin on ANY delegate
                    // height change feeds the churn→pin→churn loop that yanks
                    // the user back to the bottom. Follow-ups happen explicitly
                    // in replyTimer where real content changes.

                    // read receipt: sits just before (left of) the sent bubble,
                    // bottom-aligned so it clears the hover timestamp/copy pill
                    Text {
                        visible: delegateRoot.showReceipt
                        text: "已读"
                        color: Theme.textDim
                        font.pixelSize: Theme.fsCaption
                        anchors.right: bubble.left
                        anchors.rightMargin: 6
                        anchors.bottom: bubble.bottom
                        anchors.bottomMargin: 1
                    }

                    // hover affordances (Telegram/lobe-chat style): timestamp +
                    // copy pill float outside the bubble, in the margin every
                    // bubble already reserves (bubbles cap at half width - 56)
                    // hover tracking WITHOUT any event grabbing: HoverHandler
                    // never accepts buttons, wheel or gestures, so the ListView
                    // keeps full control of scrolling (a MouseArea here, even a
                    // no-button one, made the list unscrollable)
                    HoverHandler {
                        id: rowHover
                    }
                    Row {
                        id: hoverTools
                        spacing: 6
                        opacity: rowHover.hovered && model.msg !== "..." ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        anchors.top: bubble.top
                        anchors.left: model.isAi ? bubble.right : undefined
                        anchors.leftMargin: model.isAi ? 6 : 0
                        anchors.right: model.isAi ? undefined : bubble.left
                        anchors.rightMargin: model.isAi ? 0 : 6
                        layoutDirection: model.isAi ? Qt.LeftToRight : Qt.RightToLeft

                        Text {
                            visible: model.ts > 0
                            text: model.ts > 0 ? Qt.formatTime(new Date(model.ts), "HH:mm") : ""
                            color: Theme.textDim
                            font.pixelSize: Theme.fsCaption
                            anchors.verticalCenter: parent.verticalCenter
                        }
                        Rectangle {
                            width: 44; height: 22; radius: 11
                            color: copyMa.pressed ? Theme.glassPress
                                 : copyMa.containsMouse ? Theme.glassHover
                                 : Theme.btnFill
                            border.color: Theme.glassBorder
                            border.width: 1
                            Text {
                                anchors.centerIn: parent
                                text: "复制"
                                color: Theme.text
                                font.pixelSize: Theme.fsCaption
                            }
                            MouseArea {
                                id: copyMa
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    proxyService.copyToClipboard(model.msg)
                                    setHeaderStatus("已复制")
                                }
                            }
                            Accessible.name: "复制这条消息"
                            Accessible.role: Accessible.Button
                        }
                    }
                }
            }
        }
        // empty chat state: avatar + invite (never a blank panel)
        Column {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter | Qt.AlignTop
            Layout.topMargin: 56
            visible: msgModel.count === 0
            spacing: Theme.sp3
            Avatar {
                size: 56
                source: chatPage.aiAvatarSource
                charText: aiService.aiName().length > 0 ? aiService.aiName().charAt(0) : "A"
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "还没有消息"
                color: Theme.textDim
                font.pixelSize: Theme.fsSmall
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "在下方输入框说点什么，开始和 " + aiService.aiName() + " 聊天吧"
                color: Theme.textMuted
                font.pixelSize: Theme.fsCaption
            }
            // starter prompts (ai-chat-ui pattern): one click sends the opener,
            // the empty state invites action instead of being a dead end
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.sp2
                Repeater {
                    model: ["今天过得怎么样？", "最近有什么开心的事吗", "陪我聊聊天吧"]
                    AppButton {
                        text: modelData
                        variant: "ghost"
                        btnHeight: 32
                        onClicked: {
                            chatInput.text = modelData
                            sendMsg()
                        }
                    }
                }
            }
        }
        ListModel { id: msgModel }

        // input bar: auto-growing multiline field (NextChat pattern) —
        // Enter sends, Shift+Enter inserts a newline, grows to ~4 lines
        Rectangle {
            id: inputBar
            Layout.fillWidth: true
            Layout.preferredHeight: inputBox.height + 22
            color: Theme.chatPanelBg
            // no outer border box — the input field itself carries the focus ring

            RowLayout {
                anchors.fill: parent
                anchors.margins: 11
                spacing: 8
                Rectangle {
                    id: inputBox
                    Layout.fillWidth: true
                    implicitHeight: Math.min(118, Math.max(38, chatInput.implicitHeight))
                    radius: 16
                    color: Theme.inputFill
                    // visible focus ring when the input is active (a11y)
                    border.color: chatInput.activeFocus ? Theme.focusRing : "transparent"
                    border.width: 1
                    TextArea {
                        id: chatInput
                        anchors.fill: parent
                        color: Theme.text
                        placeholderText: "输入消息..."
                        placeholderTextColor: Theme.textDim
                        background: null
                        font.pixelSize: Theme.fsBody
                        wrapMode: TextArea.Wrap
                        leftPadding: 16
                        rightPadding: 16
                        topPadding: 10
                        bottomPadding: 6
                        Accessible.name: "消息输入框"
                        Keys.onPressed: function(event) {
                            if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                    && (event.modifiers & Qt.ShiftModifier) === 0) {
                                sendMsg()
                                event.accepted = true
                            }
                        }
                        onTextChanged: {
                            if (text.length > 0) appCore.setStatus("用户输入中...")
                            else appCore.setStatus("在线")
                        }
                    }
                }
                // fixed-width send button (no layout shift while replying)
                AppButton {
                    text: replyBusy ? "回复中" : "发送"
                    variant: "primary"
                    implicitWidth: 76
                    implicitHeight: 38
                    onClicked: sendMsg()
                }
            }
        }
    }

    // jump to latest (NextChat/lobe-chat pattern): appears once the user
    // scrolled away from the bottom, returns the view to the newest message
    Rectangle {
        id: jumpDown
        width: 32; height: 32; radius: 16
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: inputBar.height + 10
        opacity: !msgView.stick && msgView.count > 0 ? 1 : 0
        scale: opacity > 0 ? 1 : 0.6
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 160; easing.type: Easing.OutBack } }
        color: Theme.cardFill
        border.color: Theme.glassBorder
        border.width: 1
        Text {
            anchors.centerIn: parent
            text: "↓"
            color: Theme.text
            font.pixelSize: 16
            font.bold: true
        }
        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: msgView.pinBottom()
        }
        Accessible.name: "回到底部"
        Accessible.role: Accessible.Button
    }

    // ts of the last message in the conversation (for gap detection)
    property var lastMsgTs: -1

    // ---- multi-message batching: every send opens a short merge window.
    // Messages typed close together are joined into ONE request, so the AI
    // reads them all at once and answers once (WeChat-style multi-send).
    // While a request is in flight, further windows queue behind it.
    property bool aiBusy: false
    property var sendBuffer: []        // messages in the current window
    property string pendingMerged: ""  // merged text waiting for AI idle

    function sendMsg() {
        var t = chatInput.text.trim()
        if (t.length === 0) return
        chatInput.text = ""

        var now = Date.now()
        // long gap (>= 10 min): show a WeChat-style time separator + tell the
        // AI how long it has been since the previous message
        var gapPrefix = ""
        var timeLabel = ""
        if (chatPage.lastMsgTs > 0 && (now - chatPage.lastMsgTs) > chatPage.gapThresholdMs) {
            timeLabel = fmtTime(now)
            gapPrefix = "[距上次消息约 " + Math.round((now - chatPage.lastMsgTs) / 60000) + " 分钟]\n"
        }
        chatPage.lastMsgTs = now

        // consecutive user bubbles stack tightly (avatar hidden after the first)
        var grouped = false
        if (msgModel.count > 0 && !msgModel.get(msgModel.count - 1).isAi)
            grouped = true
        // sending always jumps to the newest message and resumes following
        msgView.stick = true
        msgModel.append({ "isAi": false, "msg": t, "timeLabel": timeLabel, "grouped": grouped, "ts": now, "receipt": "" })
        chatPage.pendingReadRows.push(msgModel.count - 1)

        chatPage.sendBuffer.push(gapPrefix + t)
        // persistence is best-effort; never let it break the chat
        try {
            var cid = currentContactId.length > 0 ? currentContactId : contactService.currentId()
            if (cid.length > 0) saveMsg(cid, false, t, now)
        } catch (e) { }
        mergeTimer.restart()
    }

    // merge window closed: ship everything typed in this window as one request
    function fireSend() {
        if (chatPage.sendBuffer.length === 0) return
        var merged = chatPage.sendBuffer.splice(0, chatPage.sendBuffer.length).join("\n")
        if (chatPage.aiBusy) {
            // a request is already in flight — queue behind it
            chatPage.pendingMerged = chatPage.pendingMerged.length > 0
                ? chatPage.pendingMerged + "\n" + merged
                : merged
            return
        }
        chatPage.aiBusy = true
        chatPage.beginRead(merged)
    }

    Timer {
        id: mergeTimer
        interval: 1500          // messages sent within this window merge into one
        repeat: false
        onTriggered: chatPage.fireSend()
    }

    // read receipt: the AI "reads" the batch after a human reading interval
    // (scales with length), marks the sent bubbles 已读, waits a beat, then
    // switches to 正在输入 and finally requests the reply.
    function beginRead(merged) {
        chatPage.pendingSendText = merged
        var ms = Math.max(600, Math.min(400 + merged.length * 30, 2000))
        readTimer.interval = ms
        readTimer.start()
    }

    Timer {
        id: readTimer
        repeat: false
        onTriggered: {
            chatPage.markMessagesRead()
            typingTimer.start()
        }
    }

    Timer {
        id: typingTimer
        interval: 450          // brief beat so 已读 is seen before 正在输入
        repeat: false
        onTriggered: {
            setHeaderStatus(aiService.aiName() + " 正在输入...")
            var m = chatPage.pendingSendText
            chatPage.pendingSendText = ""
            if (m.length > 0) {
                // remember which conversation this request belongs to, so a late
                // reply can never surface in a different contact's chat
                chatPage.pendingReplyContacts.push(chatPage.currentContactId)
                aiService.sendMessage(m)
            }
        }
    }

    function markMessagesRead() {
        var rows = chatPage.pendingReadRows
        for (var i = 0; i < rows.length; i++) {
            var r = rows[i]
            if (r >= 0 && r < msgModel.count && !msgModel.get(r).isAi)
                msgModel.setProperty(r, "receipt", "已读")
        }
        chatPage.pendingReadRows = []
    }

    // a "已读" receipt only means the AI has read the message; once the reply
    // is delivered it is stale, so drop it from every sent bubble
    function clearReceipts() {
        for (var i = 0; i < msgModel.count; i++) {
            if (!msgModel.get(i).isAi && msgModel.get(i).receipt === "已读")
                msgModel.setProperty(i, "receipt", "")
        }
    }

    function setHeaderStatus(s) {
        // offline presence always wins over transient chat states
        if (chatPage.apiOffline && s !== "离线") return
        headerStatus.text = s
        headerStatus.color = (s === "在线") ? Theme.ok
                           : (s === "离线") ? Theme.textDim
                           : Theme.warn
        statusDot.color = headerStatus.color
    }

    // presence: no API key filled in, or the endpoint did not answer -> 离线
    property bool apiOffline: false
    function refreshApiStatus() {
        var offline = !aiService.apiConfigured() || !aiService.apiOnline()
        chatPage.apiOffline = offline
        if (offline)
            chatPage.setHeaderStatus("离线")
        else if (headerStatus.text === "离线")
            chatPage.setHeaderStatus("在线")
    }

    // human-like reply delay: wait "typing time" proportional to text length,
    // then reveal the full reply at once. Each reply is processed one after
    // another; rapid consecutive messages never cancel an earlier reply.
    // Multi-line replies are split into separate short bubbles (like chat apps).
    function appendAi(text) {
        // a reply whose conversation the user has since left must not appear in
        // the current chat — persist it to its own contact and drop it here.
        var target = chatPage.pendingReplyContacts.length > 0
                   ? chatPage.pendingReplyContacts.shift() : ""
        if (target !== "" && target !== chatPage.currentContactId) {
            try { saveMsg(target, true, text, Date.now()) } catch (e) { }
            return
        }
        // split into lines (trim empty), each becomes its own bubble
        var parts = text.split(/\r?\n/).map(function(s) { return s.trim() }).filter(function(s) { return s.length > 0 })
        if (parts.length === 0) parts = [text]

        // no placeholder bubbles — replies simply appear after their
        // "typing" delay (real-chat feel, nothing shows until it's sent)
        for (var p = 0; p < parts.length; p++) {
            // typing speed ~ 120ms per char, clamp to 1.5s ~ 12s
            var ms = Math.round(parts[p].length * 120)
            ms = Math.max(1500, Math.min(ms, 12000))
            replyQueue.push({ "text": parts[p], "ms": ms })
        }
        setHeaderStatus(aiService.aiName() + " 正在输入...")
        pumpReplies()
    }

    // direct AI message insert (e.g. morning greeting): shows immediately
    // (no typing queue) AND persists to the chat DB so it appears in history
    function insertAiMessage(text) {
        if (!text || text.trim().length === 0) return
        chatPage.clearReceipts()
        msgModel.append({ "isAi": true, "msg": text, "timeLabel": "", "grouped": false, "ts": Date.now(), "receipt": "" })
        try {
            var cid = chatPage.currentContactId.length > 0 ? chatPage.currentContactId : contactService.currentId()
            if (cid.length > 0) {
                var db = chatDb()
                db.transaction(function(tx) {
                    tx.executeSql("CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, contact TEXT, isAi INTEGER, msg TEXT, ts INTEGER)")
                    try { tx.executeSql("ALTER TABLE messages ADD COLUMN ts INTEGER") } catch (e) { }
                    tx.executeSql("INSERT INTO messages (contact, isAi, msg, ts) VALUES (?,1,?,?)", [cid, text, Date.now()])
                })
                chatPage.messageSaved(cid, true, text)
            }
        } catch (e) { }
    }

    property var replyQueue: []
    property bool replyBusy: false
    // contact id per in-flight AI request (FIFO) — routes late replies
    property var pendingReplyContacts: []

    function pumpReplies() {
        // nothing pending, nothing running -> done
        if (replyQueue.length === 0) {
            if (!replyBusy) setHeaderStatus("在线")
            return
        }
        // a reply is currently being typed out; it will pump the next one
        if (replyBusy) return
        replyBusy = true
        var item = replyQueue.shift()
        chatPage.pendingReply = item.text
        // in-stream typing bubble (Telegram/WeChat style): a "..." placeholder
        // that the reply REPLACES when the typing delay elapses. Consecutive
        // parts of one reply group their avatars like any other AI message.
        var grouped = msgModel.count > 0 && msgModel.get(msgModel.count - 1).isAi
        msgModel.append({ "isAi": true, "msg": "...", "timeLabel": "", "grouped": grouped, "ts": Date.now(), "receipt": "" })
        chatPage.typingRow = msgModel.count - 1
        replyTimer.interval = item.ms
        replyTimer.start()
    }

    Timer {
        id: replyTimer
        repeat: false
        onTriggered: {
            replyBusy = false
            // reveal this reply: the typing placeholder becomes the message in
            // place (bubblePop plays on the text change). Fallback: append a
            // fresh row if the placeholder row vanished (contact switch etc).
            var text = chatPage.pendingReply
            var revealed = false
            if (chatPage.typingRow >= 0 && chatPage.typingRow < msgModel.count) {
                var r = msgModel.get(chatPage.typingRow)
                if (r.isAi && r.msg === "...") {
                    msgModel.setProperty(chatPage.typingRow, "msg", text)
                    msgModel.setProperty(chatPage.typingRow, "ts", Date.now())
                    revealed = true
                }
            }
            chatPage.typingRow = -1
            if (!revealed)
                msgModel.append({ "isAi": true, "msg": text, "timeLabel": "", "grouped": false, "ts": Date.now(), "receipt": "" })
            // the reply has landed — the "已读" receipt is stale now
            chatPage.clearReceipts()
            // the reveal grew the bubble (wrapped text) without changing the
            // model count, so follow explicitly if the user is still at the end
            // NOTE: followBottom() lives on the ListView (msgView), not on chatPage
            msgView.followBottom()
            // persistence (best-effort)
            try {
                var cid = chatPage.currentContactId.length > 0 ? chatPage.currentContactId : contactService.currentId()
                if (cid.length > 0) {
                    var db = chatDb()
                    db.transaction(function(tx) {
                        tx.executeSql("CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, contact TEXT, isAi INTEGER, msg TEXT, ts INTEGER)")
                        try { tx.executeSql("ALTER TABLE messages ADD COLUMN ts INTEGER") } catch (e) { }
                        tx.executeSql("INSERT INTO messages (contact, isAi, msg, ts) VALUES (?,1,?,?)", [cid, text, Date.now()])
                    })
                    chatPage.messageSaved(cid, true, text)
                }
            } catch (e) { }
            // process the next queued reply (if any)
            chatPage.pumpReplies()
            // all replies done -> free the busy flag and send queued merges
            if (!replyBusy && replyQueue.length === 0) {
                chatPage.aiBusy = false
                if (chatPage.pendingMerged.length > 0) {
                    var queued = chatPage.pendingMerged
                    chatPage.pendingMerged = ""
                    chatPage.aiBusy = true
                    chatPage.beginRead(queued)
                }
            }
        }
    }

    property string pendingReply: ""
    // model row currently showing the in-stream typing bubble ("...")
    property int typingRow: -1
    // read receipt: user-bubble rows awaiting the AI's 已读, and the text
    // staged between the read beat and the actual reply request
    property var pendingReadRows: []
    property string pendingSendText: ""

    // exposed: AI avatar image path (empty = char avatar)
    property string aiAvatarSource: ""
    property string userAvatarSource: ""

    // ---- AI emotion state (character glyphs — project rule: no emoji icons) ----
    property string emotionEmoji: ""          // single CJK glyph, e.g. "喜"
    property color emotionColor: Theme.ok
    property var emotionChars: ({
        "happy": "喜", "sad": "忧", "angry": "怒", "think": "思",
        "surprised": "惊", "awkward": "尬", "question": "疑",
        "curious": "奇", "neutral": "平", "love": "爱", "tired": "倦"
    })
    property var emotionColors: ({
        "happy": "#5FA87A", "sad": "#C55A5A", "angry": "#C55A5A", "think": "#C9A15A",
        "surprised": "#C9A15A", "awkward": "#9A9A9A", "question": "#C9A15A",
        "curious": "#C9A15A", "neutral": "#9A9A9A", "love": "#C55A5A", "tired": "#C9A15A"
    })

    function playEmotion(name, intensity) {
        var c = emotionChars[name]
        if (!c) return
        emotionEmoji = c
        emotionColor = emotionColors[name] || Theme.ok
        emotionPop.stop()
        emotionFade.stop()
        emotionPop.start()
        emotionFade.start()
    }

    Connections {
        target: aiService
        function onEmotionSignal(name, intensity) {
            chatPage.playEmotion(name, intensity)
        }
        function onApiStatusChanged() {
            chatPage.refreshApiStatus()
        }
    }

    Component.onCompleted: chatPage.refreshApiStatus()
}
