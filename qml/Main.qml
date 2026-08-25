import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.LocalStorage
import QtQuick.Effects
import "Pages"
import "Components"

ApplicationWindow {
    id: root
    width: 1100
    height: 720
    minimumWidth: 900
    minimumHeight: 600
    visible: true
    title: "小钦的工具"
    color: Theme.bg   // opaque — DWM rounds the frameless window corners natively

    // frameless: the native title bar is replaced by the custom title strip
    // below so the window chrome blends into the app (Discord/Telegram style).
    flags: Qt.Window | Qt.FramelessWindowHint

    // ---- frameless helpers ----
    function isMaximized() { return root.visibility === Window.Maximized }
    function toggleMaximize() {
        if (root.isMaximized()) root.showNormal()
        else root.showMaximized()
    }

    // Global dark palette so Quick Controls render dark.
    palette {
        window: "#141414"
        windowText: "#F0F0F0"
        base: "#262626"
        alternateBase: "#1E1E1E"
        text: "#F0F0F0"
        button: "#3A3F4A"
        buttonText: "#FFFFFF"
        highlight: "#3A3F4A"
        highlightedText: "#FFFFFF"
        toolTipBase: "#1E1E1E"
        toolTipText: "#F0F0F0"
        placeholderText: "#9A9A9A"
    }

    property int currentPage: 0
    property string aiGreeting: "你好。"
    property bool userBtnHover: false   // sidebar user-avatar ⌄ hover flag

    // time-of-day greeting under the user avatar (refreshed by greetTimer)
    function timeGreeting() {
        var h = new Date().getHours()
        if (h >= 5 && h < 11) return "早上好，愿你今天幸运伴身。"
        if (h >= 11 && h < 14) return "中午好，记得按时吃午饭哦。"
        if (h >= 14 && h < 18) return "下午好，愿你今天一切顺利。"
        if (h >= 18 && h < 23) return "晚上好，愿你今晚安心入睡。"
        return "夜深了，早点休息哦。"
    }

    // custom wallpaper (ORIGINAL image — frosted glass is live GPU blur)
    // shown behind the right pane. Layer 1 only — overlays + near-opaque UI sit ABOVE it.
    property string wallpaperUrl: ""
    property real wallpaperBrightness: 0.5    // 0..1 avg luminance, drives the adaptive scrim

    // ---- live glass mapping (blur strength -> MultiEffect) ----
    // MultiEffect.blur is NORMALIZED 0..1 (radius px = blur × blurMax):
    // slider r -> r/64 of blurMax(64) -> r pixels of real blur.
    // (Verified against Qt docs: blur 0..1, blurMax = pixel radius at blur 1.0.
    //  Feeding raw px (1,3) overflowed the range -> full fog / black.)
    property real wallBlurRadiusPx: 0
    // ±0.08 brightness compensation: dark wallpaper lifted, bright wallpaper pressed.
    // MultiEffect.brightness is an ABSOLUTE value (0=black, 1.0=unchanged, 2=max).
    readonly property real wallBlurBrightness: qBound(-0.08, (0.5 - root.wallpaperBrightness) * 0.16, 0.08)
    // wallpaper layer opacity: glass mode respects the persisted
    // wallpaper_glass_opacity (0.05..0.20, default 0.10 — the ambient look
    // users tuned since 4.1.0; 0.60 hardcode was the "overexposure" bug).
    // Dark mode stays 20% ambient. Dynamic so appearance switches never flash.
    property real wallGlassOpacity: 0.10
    readonly property real glassTargetOpacity: Theme.glassMode ? root.wallGlassOpacity : 0.20

    function refreshBlurStrength() {
        if (!aiService.wallpaperBlurEnabled()) { root.wallBlurRadiusPx = 0; return }
        root.wallBlurRadiusPx = aiService.wallpaperBlurRadius()
    }
    function refreshWallpaper() {
        var oldUrl = root.wallpaperUrl
        root.wallpaperUrl = aiService.wallpaperPath()
        root.wallpaperBrightness = aiService.wallpaperBrightness()
        Theme.wallpaperActive = root.wallpaperUrl.length > 0
        Theme.wallpaperTint = aiService.wallpaperTintColor()   // environment tint for glass mode
        root.syncAppearance()
        // crossfade only when switching between two real wallpapers (300ms)
        if (oldUrl.length > 0 && root.wallpaperUrl.length > 0 && oldUrl !== root.wallpaperUrl) {
            wpFront.source = root.wallpaperUrl
            wpFront.opacity = 0
            wpCrossfade.start()
        } else if (oldUrl.length === 0 && root.wallpaperUrl.length > 0) {
            wpBack.source = root.wallpaperUrl   // first set: just show
        } else if (root.wallpaperUrl.length === 0) {
            wpBack.source = ""                  // wallpaper removed
        }
    }
    // sync the persisted appearance mode into the Theme singleton so every
    // Theme.* token re-renders (real-time switching works from SettingsPage)
    function syncAppearance() {
        Theme.appearanceMode = aiService.appearanceMode()
        Theme.glassOpacity = aiService.wallpaperGlassOpacity()
        root.wallGlassOpacity = aiService.wallpaperGlassOpacity()
    }
    Connections {
        target: aiService
        function onWallpaperChanged() { root.refreshWallpaper() }
        function onWallpaperBlurChanged() { root.refreshBlurStrength() }
    }

    // contact list model (id|name|hasAvatar)
    ListModel { id: contactModel }

    // read the last message of a contact from the chat DB (preview)
    function lastMsgFor(contactId) {
        try {
            var db = LocalStorage.openDatabaseSync("XiaoQinChat", "1.0", "chat history", 8*1024*1024)
            var preview = ""
            db.transaction(function(tx) {
                tx.executeSql("CREATE TABLE IF NOT EXISTS messages (id INTEGER PRIMARY KEY AUTOINCREMENT, contact TEXT, isAi INTEGER, msg TEXT)")
                var rs = tx.executeSql("SELECT isAi, msg FROM messages WHERE contact=? ORDER BY id DESC LIMIT 1", [contactId])
                if (rs.rows.length > 0) {
                    var m = rs.rows.item(0)
                    preview = (m.isAi === 1 ? "" : "我: ") + m.msg
                }
            })
            return preview
        } catch (e) { return "" }
    }

    function refreshContacts() {
        contactModel.clear()
        var list = contactService.contactList()
        for (var i = 0; i < list.length; i++) {
            var parts = list[i].split("|")
            if (parts.length >= 3) {
                var avatarUrl = ""
                if (parts[2] === "1")
                    avatarUrl = contactService.contactAvatarUrl(parts[0])
                var preview = lastMsgFor(parts[0])
                contactModel.append({ "cid": parts[0], "cname": parts[1], "hasAvatar": parts[2], "avatarUrl": avatarUrl, "lastMsg": preview })
            }
        }
    }

    // refresh profile labels after edit
    function refreshProfile() {
        userAvatarText.text = aiService.avatarChar()
        userNameText.text = aiService.userName()
        root.aiAvatarPath = toFileUrl(aiService.aiAvatarPath())
        root.userAvatarPath = toFileUrl(aiService.userAvatarPath())
        refreshContacts()
    }

    // convert a local path to a file:// URL usable by Image.source
    function toFileUrl(p) {
        if (p.length === 0) return ""
        if (p.indexOf("file://") === 0) return p
        return "file:///" + p.replace(/\\/g, "/")
    }

    // ================= ROUNDED WINDOW FRAME =================
    // frameless window; Windows 11 DWM rounds the corners natively (opaque
    // window, so the corner clip is handled by the OS).
    Rectangle {
        id: rootPanel
        anchors.fill: parent
        radius: Theme.rXl
        color: Theme.bg
        border.color: Qt.rgba(255,255,255,0.07)
        border.width: 1
        clip: true

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ================= CUSTOM TITLE STRIP (frameless) =================
        // blends into the app: dark, matches the sidebar tone, window controls
        // on the right (Discord/Telegram/Linear style).
        Rectangle {
            id: titleBar
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.sidebar

            // drag region (whole strip minus the control buttons)
            MouseArea {
                id: titleDrag
                anchors.fill: parent
                anchors.right: winControls.left
                cursorShape: Qt.ArrowCursor
                onPressed: function(mouse) { if (mouse.button === Qt.LeftButton) root.startSystemMove() }
                onDoubleClicked: root.toggleMaximize()
            }

            // subtle hairline under the strip
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Theme.glassBorder
            }

            // app icon + title — blends over the sidebar zone (restores the
            // icon that the native title bar used to show top-left)
            Image {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 16; height: 16
                source: "qrc:/icons/app.ico"
                fillMode: Image.PreserveAspectFit
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 38
                anchors.verticalCenter: parent.verticalCenter
                text: "小钦的工具"
                color: Theme.textDim
                font.pixelSize: Theme.fsCaption
                font.bold: true
            }

            // window controls: minimize / maximize / close
            Row {
                id: winControls
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                spacing: 0
                WindowButton { variant: "min"; tip: "最小化"; onClicked: root.showMinimized() }
                WindowButton {
                    variant: root.isMaximized() ? "restore" : "max"
                    tip: root.isMaximized() ? "还原" : "最大化"
                    onClicked: root.toggleMaximize()
                }
                WindowButton { variant: "close"; tip: "关闭"; onClicked: Qt.quit() }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ================= LEFT NAVIGATION =================
            Rectangle {
                Layout.preferredWidth: 248
                Layout.fillHeight: true
                color: Theme.sidebar

                ColumnLayout {
                    anchors.fill: parent
                spacing: 0

                // ---- top: user avatar + name ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 92
                    color: "transparent"
                    RowLayout {
                        anchors.top: parent.top
                        anchors.topMargin: 12
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        spacing: 12

                        Rectangle {
                            id: userAvatarBox
                            width: 42; height: 42
                            radius: 21
                            color: Theme.accent
                            clip: true
                            scale: userBtn.pressed ? 0.92 : 1.0
                            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                            Image {
                                anchors.fill: parent
                                visible: root.userAvatarPath.length > 0
                                source: root.userAvatarPath
                                fillMode: Image.PreserveAspectCrop
                                smooth: true
                                mipmap: true
                            }
                            Text {
                                id: userAvatarText
                                anchors.centerIn: parent
                                text: root.userAvatarPath.length > 0 ? "" : aiService.avatarChar()
                                color: "white"
                                font.pixelSize: 17
                                font.bold: true
                            }
                        }

                        Column {
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2
                            Text {
                                id: userNameText
                                text: aiService.userName()
                                color: Theme.navText
                                font.pixelSize: 15
                                font.bold: true
                            }
                            Row {
                                spacing: 6
                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.ok
                                }
                                Text { text: appCore.statusText; color: Theme.navTextDim; font.pixelSize: 11 }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // shows on hover — uses an explicit flag (onEntered/onExited)
                        // because MouseArea.hovered is unavailable in this Qt build
                        Text {
                            text: "⌄"
                            color: Theme.navTextDim
                            font.pixelSize: 16
                            visible: root.userBtnHover
                            opacity: root.userBtnHover ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: 150 } }
                        }
                    }
                    MouseArea {
                        id: userBtn
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: root.userBtnHover = true
                        onExited: root.userBtnHover = false
                        onClicked: userMenu.popup(userBtn, 0, userBtn.height + 4)
                    }

                    // time-of-day greeting: inside the header block, anchored
                    // to its bottom-left, left-aligned with the avatar
                    Text {
                        id: timeGreetText
                        anchors.left: parent.left
                        anchors.leftMargin: 18
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 14
                        text: root.timeGreeting()
                        color: Theme.navTextDim
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        wrapMode: Text.NoWrap
                        opacity: 0.85
                    }
                }

                // user menu
                Menu {
                    id: userMenu
                    width: 200
                    background: Rectangle {
                        color: Theme.cardFill
                        radius: 10
                        border.color: Theme.glassBorder
                        border.width: 1
                    }
                    MenuItem {
                        text: "编 辑资料"
                        onClicked: profileDialog.open()
                    }
                    MenuItem {
                        text: "设 置"
                        onClicked: pageStack.switchPage(3)
                    }
                    MenuItem {
                        text: "导 出配置"
                        onClicked: {
                            var ok = syncService.exportConfig(syncService.defaultExportPath())
                            islandToast.show(ok ? "配置已导出到杂货铺" : "导出失败")
                        }
                    }
                    MenuItem {
                        text: "退 出"
                        onClicked: Qt.quit()
                    }
                }

                Rectangle { // separator
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.glassBorder
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                }

                // ================= AI CONTACTS =================
                // contact list
                Repeater {
                    model: contactModel
                    delegate: Rectangle {
                        id: contactItem
                        Layout.fillWidth: true
                        Layout.preferredHeight: 58
                        Layout.leftMargin: Theme.sp2
                        Layout.rightMargin: Theme.sp2
                        radius: Theme.rMd
                        // selected contact: only a thin left indicator bar (no full highlight)
                        color: cHover ? Theme.hoverBg : "transparent"
                        property bool cHover: false
                        Behavior on color { ColorAnimation { duration: 140 } }

                        // selected indicator (small vertical bar, like nav tabs)
                        Rectangle {
                            width: 3; height: cid === contactService.currentId() ? 24 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            color: Theme.navTextDim
                            radius: 1.5
                            opacity: cid === contactService.currentId() ? 1 : 0
                            Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                            Behavior on opacity { NumberAnimation { duration: 180 } }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 12
                            spacing: 10
                            Rectangle {
                                width: 36; height: 36
                                radius: 18
                                color: Theme.accent
                                clip: true
                                antialiasing: true
                                smooth: true
                                Image {
                                    anchors.fill: parent
                                    visible: avatarUrl.length > 0
                                    source: avatarUrl
                                    fillMode: Image.PreserveAspectCrop
                                    smooth: true
                                    mipmap: true
                                }
                                Text {
                                    anchors.centerIn: parent
                                    visible: avatarUrl.length === 0
                                    text: cname.length > 0 ? cname.charAt(0) : "A"
                                    color: "white"
                                    font.pixelSize: 14
                                    font.bold: true
                                }
                                // unread red dot (proactive AI messages while user is away)
                                Rectangle {
                                    width: 10; height: 10
                                    radius: 5
                                    color: "#E5534B"
                                    border.color: Theme.sidebar
                                    border.width: 1
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.rightMargin: -1
                                    anchors.topMargin: -1
                                    visible: cid === contactService.currentId() && root.unreadCount > 0 && (!root.chatOpen || !root.active)
                                }
                            }
                            Column {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    text: cname
                                    color: Theme.navText
                                    font.pixelSize: 13
                                    font.bold: cid === contactService.currentId()
                                }
                                // live conversation preview (last message), falls back to a hint
                                Text {
                                    text: lastMsg.length > 0 ? lastMsg : "点击开始聊天"
                                    color: Theme.navTextDim
                                    font.pixelSize: 10
                                    elide: Text.ElideRight
                                    maximumLineCount: 1
                                    width: parent.width
                                }
                            }
                            Text {
                                text: "›"
                                color: Theme.navTextDim
                                font.pixelSize: 15
                            }
                        }
                        // MouseArea last = on top, so clicks always reach it
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: contactItem.cHover = true
                            onExited: contactItem.cHover = false
                            onClicked: {
                                // open the chat first, then refresh profile:
                                // setCurrent fires contactsChanged -> refreshProfile (reentrant),
                                // so chatOpen must come before it or the signal chain breaks.
                                contactService.setCurrent(cid)
                                root.chatOpen = true
                                chatPage.openContact(cid)
                                root.clearUnread()
                                root.refreshProfile()
                            }
                        }
                    }
                }

                // empty state: no AI contacts yet — explain + CTA
                Column {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.sp6
                    visible: contactModel.count === 0
                    spacing: Theme.sp3
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "还没有 AI 联系人"
                        color: Theme.navTextDim
                        font.pixelSize: Theme.fsSmall
                    }
                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "去「设置 → AI 配置」添加第一个 AI 吧"
                        color: Theme.navTextMuted
                        font.pixelSize: Theme.fsCaption
                    }
                }

                Rectangle { // separator
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.glassBorder
                    Layout.leftMargin: 14
                    Layout.rightMargin: 14
                }

                Item { Layout.fillHeight: true }

                // ---- nav items (lower-middle) — continuous sidebar list ----
                Repeater {
                    model: ListModel {
                        ListElement { label: "网络" }
                        ListElement { label: "系统" }
                        ListElement { label: "娱乐" }
                        ListElement { label: "设置" }
                    }
                    Rectangle {
                        id: navItem
                        property bool hovered: false
                        property int myIndex: index
                        // when the chat page is open, no nav tab stays highlighted
                        readonly property bool active: !root.chatOpen && root.currentPage === myIndex
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        // continuous list: transparent default, faint hover tint,
                        // single flat active block (no border / no outline / no card)
                        color: active ? Qt.rgba(1,1,1,0.06)
                             : hovered ? Qt.rgba(1,1,1,0.035)
                             : "transparent"
                        Behavior on color { ColorAnimation { duration: 140 } }
                        focus: true

                        // selected: left thin line (Linear/macOS settings style)
                        Rectangle {
                            width: 2; height: active ? 20 : 0
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            color: Theme.navText
                            radius: 1
                            opacity: active ? 1 : 0
                            Behavior on height { NumberAnimation { duration: Theme.durMid; easing.type: Easing.OutCubic } }
                            Behavior on opacity { NumberAnimation { duration: Theme.durMid } }
                        }

                        Keys.onReturnPressed: activate()
                        Keys.onEnterPressed: activate()
                        Keys.onSpacePressed: activate()
                         function activate() {
                            pageStack.switchPage(myIndex)
                            root.chatOpen = false
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Theme.sp4
                            anchors.rightMargin: Theme.sp3
                            spacing: Theme.sp3
                            Text {
                                text: model.label
                                color: active ? Theme.navText : Theme.navTextDim
                                font.pixelSize: Theme.fsDefault
                                font.weight: active ? Font.DemiBold : Font.Normal
                            }
                            Item { Layout.fillWidth: true }
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: navItem.hovered = true
                            onExited: navItem.hovered = false
                            onClicked: navItem.activate()
                        }
                    }
                }

                // ---- bottom: version line ----
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    color: "transparent"
                    Text {
                        anchors.centerIn: parent
                        text: "小钦的工具 v" + updateService.currentVersion()
                        color: Theme.navTextDim
                        font.pixelSize: Theme.fsCaption
                    }
                }
            }
        }

        // ================= MAIN CONTENT =================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            // ================= LAYER 1: WALLPAPER BACKGROUND =================
            // Real-time GPU frosted glass: the ORIGINAL image is blurred live
            // by MultiEffect (QtQuick.Effects) — no pre-blurred PNG copy, no
            // disk writes, no regeneration. The 300ms crossfade still applies.
            Item {
                id: wallStack
                anchors.fill: parent
                // rendered ONLY via wallGrab -> MultiEffect (single pass).
                // Never shown directly: the effect would stack on top of it.
                visible: false
                Image {
                    id: wpBack
                    anchors.fill: parent
                    source: root.wallpaperUrl
                    sourceSize: Qt.size(Math.ceil(root.width), Math.ceil(root.height))
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                }
                Image {
                    id: wpFront
                    anchors.fill: parent
                    source: ""
                    sourceSize: Qt.size(Math.ceil(root.width), Math.ceil(root.height))
                    fillMode: Image.PreserveAspectCrop
                    smooth: true
                    mipmap: true
                    opacity: 0
                }
                SequentialAnimation {
                    id: wpCrossfade
                    NumberAnimation { target: wpFront; property: "opacity"; from: 0; to: 1.0; duration: 300; easing.type: Easing.OutCubic }
                    ScriptAction { script: { wpBack.source = wpFront.source; wpFront.source = ""; wpFront.opacity = 0 } }
                }
            }

            // bridge: captures the (hidden) wallpaper stack into a texture —
            // the documented way to sample invisible items. Live so the
            // crossfade and appearance switches flow through. The grab texture
            // is FULLY OPAQUE: MultiEffect outputs it without the source's
            // alpha, so the wallpaper's final opacity (20%/60%) must live on
            // the effect item itself — otherwise the wallpaper doubles up and
            // blows out.
            ShaderEffectSource {
                id: wallGrab
                anchors.fill: parent
                sourceItem: wallStack
                visible: false
                live: true
                smooth: true
                textureSize: Qt.size(Math.ceil(root.width), Math.ceil(root.height))
            }

            // live gaussian blur over the grabbed wallpaper (single GPU pass).
            // blur (0..1 normalized) = slider r / blurMax(64) -> r px radius.
            // 0 when 模糊背景 is off. MultiEffect runs on every RHI backend
            // (D3D11/OpenGL/Metal/Vulkan) — no GraphicsInfo gating needed.
            MultiEffect {
                anchors.fill: parent
                visible: root.wallpaperUrl.length > 0
                opacity: root.glassTargetOpacity   // 0.20 dark / config glass — applied HERE, not inside the grab
                source: wallGrab
                blurEnabled: root.wallBlurRadiusPx > 0
                blurMax: 64
                blur: qBound(0.0, root.wallBlurRadiusPx / 64.0, 1.0)
                saturation: 1.15                    // compensation for the overlay grey-out
                brightness: 1.0 + root.wallBlurBrightness  // ±0.08 by wallpaper luminance
            }

// ================= LAYER 2: OVERLAY =================
            // Dark mode: dark overlay adapting to wallpaper brightness.
            // Glass mode: brightness-adaptive black scrim (dark wallpaper barely
            // dimmed, bright wallpaper pressed up to glassScrimMax) so white
            // text stays readable on bright wallpapers.
            Rectangle {
                anchors.fill: parent
                color: root.wallpaperUrl.length > 0
                     ? Qt.rgba(0, 0, 0, Theme.glassMode
                                           ? qBound(Theme.glassScrimMin,
                                                    Theme.glassScrimMin + Math.max(0, root.wallpaperBrightness - 0.55) * Theme.glassScrimK,
                                                    Theme.glassScrimMax)
                                           : 0.45 + 0.20 * root.wallpaperBrightness)
                     : "transparent"
            }
            // glass mode: environment tint from the wallpaper itself (≤8%),
            // never a hardcoded blue wash
            Rectangle {
                anchors.fill: parent
                visible: Theme.glassMode && root.wallpaperUrl.length > 0
                color: Qt.rgba(Theme.wallpaperTint.r, Theme.wallpaperTint.g, Theme.wallpaperTint.b, Theme.glassTintAlpha)
            }

            // subtle vignette: darken the extreme top/bottom edges so cards and
            // the header pop (depth, macOS dark-desktop feel). No white/glass.
            Rectangle {
                anchors.fill: parent
                visible: root.wallpaperUrl.length > 0
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Qt.rgba(0, 0, 0, Theme.vignetteAlpha) }
                    GradientStop { position: 0.20; color: Qt.rgba(0, 0, 0, 0.0) }
                    GradientStop { position: 0.80; color: Qt.rgba(0, 0, 0, 0.0) }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, Theme.vignetteAlpha) }
                }
            }

            StackLayout {
                id: pageStack
                anchors.fill: parent
                currentIndex: root.currentPage
                // LAYER 3 is hidden while the chat overlay (LAYER 4) is open, so
                // business pages never show through the chat.
                visible: !root.chatOpen

                onCurrentIndexChanged: {
                    // NOTE: pageStack.currentItem is UNDEFINED in this handler;
                    // use itemAt(currentIndex) — that's why page switches had no
                    // animation before.
                    var item = pageStack.itemAt(pageStack.currentIndex)
                    if (item) {
                        item.opacity = 0
                        item.scale = 0.96
                        itemBehavior.target = item
                        itemBehavior.restart()
                    }
                }

                SequentialAnimation {
                    id: itemBehavior
                    property Item target
                    ParallelAnimation {
                        NumberAnimation { target: itemBehavior.target; property: "opacity"; to: 1; duration: 170; easing.type: Easing.OutCubic }
                        NumberAnimation { target: itemBehavior.target; property: "scale"; to: 1.0; duration: 170; easing.type: Easing.OutCubic }
                    }
                }

                // ---- page exit: fade out the current page, then switch ----
                // The outgoing page must animate BEFORE currentIndex changes
                // (StackLayout hides it instantly on switch), so switching goes
                // through switchPage() which runs this first.
                property int pendingPage: -1
                function switchPage(idx) {
                    if (idx === root.currentPage) return
                    var out = pageStack.itemAt(pageStack.currentIndex)
                    if (out) {
                        // NOTE: unqualified `pendingPage` is NOT visible inside
                        // nested signal handlers (ReferenceError -> frozen UI).
                        // Qualify explicitly.
                        pageStack.pendingPage = idx
                        pageOutAnim.target = out
                        pageOutAnim.restart()
                    } else {
                        root.currentPage = idx
                    }
                }
                ParallelAnimation {
                    id: pageOutAnim
                    property Item target
                    // fast exit (quicker than enter, quick start) so switching
                    // never feels sticky: 110ms OutCubic reads as a brisk
                    // fade-out instead of a slow drag.
                    NumberAnimation { target: pageOutAnim.target; property: "opacity"; to: 0; duration: 110; easing.type: Easing.OutCubic }
                    NumberAnimation { target: pageOutAnim.target; property: "scale"; to: 0.98; duration: 110; easing.type: Easing.OutCubic }
                    onFinished: {
                        root.currentPage = pageStack.pendingPage
                        pageStack.pendingPage = -1
                    }
                }

                NetworkPage {}
                SystemPage {}
                EntertainmentPage {}
                SettingsPage {}
            }

            // Chat page overlays the right content area (sidebar stays visible)
            ChatPage {
                id: chatPage
                anchors.fill: parent
                visible: root.chatOpen
                aiAvatarSource: root.aiAvatarPath
                userAvatarSource: root.userAvatarPath
                onBackRequested: chatPage.closeWithAnim()
                onCloseFinished: root.chatOpen = false
                onAiProfileRequested: aiProfileDialogs.openProfile()
            }
        }
    }
    } // end ColumnLayout (title strip + content)
    } // end rootPanel (rounded window frame)

    // ===== frameless resize handles (native startSystemResize) =====
    // invisible strips on the window edges so a frameless window can be resized
    Rectangle {
        width: parent.width; height: 6
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        color: "transparent"; z: 20
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.SizeVerCursor
            onPressed: root.startSystemResize(Qt.BottomEdge)
        }
    }
    Rectangle {
        width: 6; height: parent.height
        anchors.right: parent.right; anchors.top: parent.top; anchors.bottom: parent.bottom
        color: "transparent"; z: 20
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.SizeHorCursor
            onPressed: root.startSystemResize(Qt.RightEdge)
        }
    }
    Rectangle {
        width: 14; height: 14
        anchors.right: parent.right; anchors.bottom: parent.bottom
        color: "transparent"; z: 20
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.SizeFDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
        }
    }
    Rectangle {
        width: 14; height: 14
        anchors.left: parent.left; anchors.bottom: parent.bottom
        color: "transparent"; z: 20
        MouseArea {
            anchors.fill: parent; cursorShape: Qt.SizeBDiagCursor
            onPressed: root.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
        }
    }

    // ================= PROFILE EDIT DIALOG =================
    ProfileEditDialog {
        id: profileDialog
        userAvatarSource: root.userAvatarPath
        onSaved: {
            root.refreshProfile()
            islandToast.show("资料已更新~")
        }
        onAvatarPickRequested: {
            fileDialog.avatarTarget = "user"
            fileDialog.open()
        }
    }

    // ================= AI PROFILE DIALOGS =================
    AiProfileDialogs {
        id: aiProfileDialogs
        aiAvatarSource: root.aiAvatarPath
        onToast: (message) => islandToast.show(message)
        onProfileSaved: root.refreshProfile()
        onAvatarPickRequested: {
            fileDialog.avatarTarget = "ai"
            fileDialog.open()
        }
    }

    // ================= ISLAND TOAST (single instance) =================
    IslandToast {
        id: islandToast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 16
        z: 999
        onActionChosen: function(index) { appCore.selectIslandAction(index) }
    }

    Connections {
        target: appCore
        function onToastRequested() {
            islandToast.show(appCore.toastMessage)
        }
        function onIslandRequested(message, options, actionId) {
            if (options.length > 0)
                islandToast.showChoice(message, options)
            else
                islandToast.show(message)
        }
        function onIslandActionChosen(actionId, index) {
            // single decision point for island actions
            if (actionId === "open_proxy") {
                var which = index === 0 ? "Clash Verge" : "v2rayN"
                appCore.setStatus("打开梯子中...")
                var ok = index === 0 ? proxyService.launchClash() : proxyService.launchV2ray()
                islandToast.show(ok ? "已打开 " + which : "打开失败")
                if (ok) statsService.record("proxy", which)
            }
        }
    }

    // chat page open state
    property bool chatOpen: false
    property bool appReady: false

    // unread badge for proactive AI messages
    property int unreadCount: 0
    function bumpUnread() {
        root.unreadCount++
    }
    function clearUnread() {
        root.unreadCount = 0
    }

    // avatar image paths (empty = char avatar), stored as file:// URLs
    property string aiAvatarPath: ""
    property string userAvatarPath: ""

    // file picker for avatar upload
    FileDialog {
        id: fileDialog
        property string avatarTarget: ""
        nameFilters: ["图片文件 (*.png *.jpg *.jpeg *.bmp *.gif)", "所有文件 (*.*)"]
        onAccepted: {
            var url = fileDialog.selectedFile.toString()
            var p = url.replace("file:///", "").replace("file://", "")
            if (p.length > 0) {
                if (fileDialog.avatarTarget === "user") {
                    aiService.setUserAvatar(p)
                    root.userAvatarPath = toFileUrl(aiService.userAvatarPath())
                } else {
                    aiService.setAiAvatar(p)
                    root.aiAvatarPath = toFileUrl(aiService.aiAvatarPath())
                }
                islandToast.show("头像已更新~")
            }
        }
    }

    // startup: greeting shown once via island toast for 20s; AI knows what was said
    Component.onCompleted: {
        root.appReady = true
        // load avatars (files may appear after first upload)
        root.aiAvatarPath = toFileUrl(aiService.aiAvatarPath())
        root.userAvatarPath = toFileUrl(aiService.userAvatarPath())
        root.refreshWallpaper()
        root.refreshContacts()
        root.aiGreeting = aiService.greeting()
        if (aiService.shouldGreetToday()) {
            // show once on the island AND persist into chat history so the
            // greeting reads like a real first message from the AI
            islandToast.show(root.aiGreeting, 20000)
            chatPage.insertAiMessage(root.aiGreeting)
            aiService.generateGreeting()
            aiService.markGreeted()
        }
        // warn when running from a stray copy (not the installed location)
        if (!appCore.isProperLocation())
            locationWarnTimer.start()
        idleTimer.start()
        restTimer.start()
    }

    Timer {
        id: locationWarnTimer
        interval: 2500
        repeat: false
        onTriggered: {
            islandToast.show("当前是旧副本，请用安装器重新安装到 Program Files", 6000)
        }
    }

    Connections {
        target: aiService
        function onChatReply(text) {
            chatPage.appendAi(text)
        }
        function onIdleReply(text) {
            // proactive message from the AI:
            // - if the window is active AND chat page is open for the current AI -> no sound / no badge
            // - otherwise (backgrounded, minimized, or chat closed) play sound and show the unread dot
            if (root.active && root.chatOpen) {
                root.clearUnread()
                return
            }
            appCore.playNotify()
            root.bumpUnread()
        }
        function onGreetingReady(text) {
            // async regeneration: keep the new text for future sessions,
            // but do NOT re-show / re-insert (already done once at startup)
            root.aiGreeting = text
        }
        function onProfileChanged() {
            root.refreshProfile()
        }
    }

    Connections {
        target: chatPage
        function onMessageSaved(contactId, isAi, msg) {
            // update the preview for this contact immediately (latest wins),
            // regardless of DB state — refresh afterwards for consistency
            for (var i = 0; i < contactModel.count; i++) {
                var it = contactModel.get(i)
                if (it.cid === contactId) {
                    var preview = (isAi ? "" : "我: ") + msg
                    contactModel.set(i, { "cid": it.cid, "cname": it.cname, "hasAvatar": it.hasAvatar, "avatarUrl": it.avatarUrl, "lastMsg": preview })
                    break
                }
            }
            // also refresh from DB so restart-time state stays consistent
            root.refreshContacts()
        }
    }

    Connections {
        target: contactService
        function onContactsChanged() {
            if (root.appReady) {
                root.refreshContacts()
                root.refreshProfile()
            }
        }
    }

    // ================= PROACTIVE CHAT (every ~30 min, randomized) =================
    // AI proactively sends a message on a 25~35 min random schedule (~30 min
    // base with jitter, so it never feels like clockwork).
    // Coding/busy foreground never interrupted; the next attempt is scheduled
    // from the moment a message was actually sent (no spam on game sessions).
    // NOTE: must be `var` (not `int`) — Date.now() is a 13-digit ms timestamp
    // that overflows QML's 32-bit int and becomes negative, which would make
    // `Date.now() >= nextProactiveAt` always true and spam proactive chats.
    property var nextProactiveAt: 0

    function scheduleNextProactive() {
        var mins = 25 + Math.floor(Math.random() * 11) // 25..35 min
        root.nextProactiveAt = Date.now() + mins * 60 * 1000
    }

    // reset the proactive schedule when the window regains focus / user interacts
    onActiveChanged: {
        if (root.active) root.scheduleNextProactive()
    }

    Timer {
        id: idleTimer
        interval: 30000
        repeat: true
        onTriggered: {
            if (!root.nextProactiveAt) root.scheduleNextProactive()
            // coding / busy foreground -> never interrupt (schedule holds)
            if (aiService.userActivityState() === "coding") return
            if (Date.now() >= root.nextProactiveAt) {
                root.scheduleNextProactive()
                // feed the AI the last 3 chat messages so it can start a
                // topic based on what was actually being discussed
                var recent = chatPage.recentMessages(3)
                if (recent.length > 0)
                    aiService.setChatHistory(recent.join("\n"))
                aiService.idleChat()
                islandToast.show(aiService.aiName() + "想找你聊聊~", 6000)
            }
        }
    }

    // ================= long-session reminder: nudge to rest after long continuous use =================
    property var appStartTime: Date.now()
    property bool restReminded: false

    // keep the sidebar time-greeting accurate across hour boundaries
    Timer {
        interval: 60000
        repeat: true
        onTriggered: timeGreetText.text = root.timeGreeting()
    }
    Timer {
        id: restTimer
        interval: 30 * 60 * 1000   // check every 30 min
        repeat: true
        onTriggered: {
            if (root.restReminded) return
            var elapsedH = (Date.now() - root.appStartTime) / 3600000
            if (elapsedH >= 3) {
                root.restReminded = true
                islandToast.show("已经连续使用 3 小时了，起来活动一下吧~", 8000)
            }
        }
    }
}
