import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Components"

// Home dashboard — a fixed, no-scroll daily briefing.
// Top: the AI 日报 card (the visual anchor). Bottom: two info cards
// (更新公告 + GitHub 今日热门).
Page {
    id: page
    padding: 0
    background: Rectangle { color: "transparent" }

    signal navigate(int index)

    // ================= report / card data =================
    property var rpt: ({})
    property string advice: ""
    property var trending: []
    property bool trendingLoaded: false
    property string releaseTag: ""
    property var releaseLines: []
    property string releaseUrl: "https://github.com/XiaoqinOvo-UwU/xiaoqintools/blob/main/CHANGELOG.md"
    property bool releaseLoaded: false
    // embedded summary (source fallback) so the card is never empty offline
    readonly property var fallbackNotes: [
        "聊天新增「已读」回执与「离线」状态",
        "修复聊天列表贴底：不漏消息、不回弹",
        "主页新增 AI 日报 / 更新公告 / GitHub 热门"
    ]

    function ymd(d) {
        var p = function(n) { return (n < 10 ? "0" : "") + n }
        return d.getFullYear() + "-" + p(d.getMonth() + 1) + "-" + p(d.getDate())
    }
    function refreshReport() {
        try { page.rpt = JSON.parse(aiService.dailyReportData()) } catch (e) { page.rpt = ({}) }
    }
    function refreshTrending() {
        var y = new Date(Date.now() - 86400000)
        var q = "created:>=" + page.ymd(y)
        netService.fetchJson("https://api.github.com/search/repositories?q=" + encodeURIComponent(q)
                             + "&sort=stars&order=desc&per_page=5", "trending")
    }
    function refreshChangelog() {
        // a dedicated file in the repo is the source of truth for "what changed"
        netService.fetchJson("https://raw.githubusercontent.com/XiaoqinOvo-UwU/xiaoqintools/main/CHANGELOG.md", "changelog")
    }
    function parseTrending(json) {
        var out = []
        try {
            var items = JSON.parse(json).items || []
            for (var i = 0; i < items.length && i < 5; i++)
                out.push({ name: items[i].full_name, stars: items[i].stargazers_count, url: items[i].html_url })
        } catch (e) { }
        page.trending = out
    }
    // read the first "## vX" section of the changelog; keep at most 3 short
    // bullets so the summary always fits the card face
    function parseChangelog(text) {
        var tag = "", notes = [], started = false
        var raw = (text || "").split("\n")
        for (var i = 0; i < raw.length; i++) {
            var s = raw[i].trim()
            if (s.indexOf("## ") === 0) { if (started) break; tag = s.substring(3).trim(); started = true; continue }
            if (started && s.indexOf("- ") === 0) {
                notes.push(s.substring(2).trim())
                if (notes.length >= 3) break
            }
        }
        if (tag.length > 0) page.releaseTag = tag
        page.releaseLines = notes.length > 0 ? notes : page.fallbackNotes
    }
    function starFmt(n) { return !n ? "0" : (n >= 1000 ? (n / 1000).toFixed(1) + "k" : String(n)) }
    function releaseStatus() {
        // the real update check (UpdateService, api.github.com) wins
        if (updateService.updateAvailable)
            return "发现新版本 " + updateService.latestVersion
        var m = page.releaseTag.match(/v?\d+\.\d+\.\d+/)
        if (m) {
            var tag = m[0].charAt(0) === "v" ? m[0] : ("v" + m[0])
            if (tag !== ("v" + updateService.currentVersion())) return "发现新版本 " + tag
        }
        return "已是最新 v" + updateService.currentVersion()
    }
    function openUrl(u) { if (u && u.length > 0) Qt.openUrlExternally(u) }

    // ================= misc helpers =================
    function timeGreeting() {
        var h = new Date().getHours()
        if (h < 5) return "夜深了"
        if (h < 11) return "早上好"
        if (h < 14) return "中午好"
        if (h < 18) return "下午好"
        if (h < 23) return "晚上好"
        return "夜深了"
    }
    function todayLine() {
        var d = new Date()
        var wd = ["周日", "周一", "周二", "周三", "周四", "周五", "周六"][d.getDay()]
        return (d.getMonth() + 1) + " 月 " + d.getDate() + " 日 · " + wd
    }
    function userName() { var n = aiService.userName(); return n.length > 0 ? n : "你" }
    function aiName() { var n = aiService.aiName(); return n.length > 0 ? n : "AI" }
    function aiOnline() { return aiService.apiConfigured() && aiService.apiOnline() }
    function rv(k, d) {
        var v = page.rpt ? page.rpt[k] : undefined
        return (v === undefined || v === null || v === "") ? d : v
    }
    function fmtDuration(m) {
        if (!m || m <= 0) return "0 分钟"
        var h = Math.floor(m / 60), mm = m % 60
        if (h > 0) return mm > 0 ? (h + " 小时 " + mm + " 分钟") : (h + " 小时")
        return mm + " 分钟"
    }

    Component.onCompleted: {
        refreshReport()
        aiService.generateDailyAdvice()
        refreshTrending()
        refreshChangelog()
    }
    onVisibleChanged: if (visible) refreshReport()

    Connections {
        target: aiService
        function onAdviceReady(text) { if (text && text.length > 0) page.advice = text }
    }
    Connections {
        target: netService
        function onJsonReady(tag, json) {
            if (tag === "trending") { page.parseTrending(json); page.trendingLoaded = true }
            else if (tag === "changelog") { page.parseChangelog(json); page.releaseLoaded = true }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.sp5
        spacing: Theme.sp4

        // ================= QUIET TOP STRIP =================
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.sp3
            Text { text: page.todayLine(); color: Theme.textDim; font.pixelSize: Theme.fsSmall }
            Item { Layout.fillWidth: true }
            StatusBadge {
                state: page.aiOnline() ? "ok" : "error"
                label: page.aiName() + (page.aiOnline() ? " 在线" : " 离线")
            }
            Text { text: "v" + updateService.currentVersion(); color: Theme.textMuted; font.pixelSize: Theme.fsCaption }
        }

        // ================= AI 日报 (dominant, fills remaining height) =================
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: reportCol.implicitHeight + Theme.sp5 * 2
            radius: Theme.rXl
            color: Theme.cardFill
            border.color: Theme.glassBorder
            border.width: 1
            clip: true

            Rectangle {
                width: 320; height: 320; radius: 160
                x: parent.width - 190; y: -180
                color: Qt.rgba(0.42, 0.48, 0.58, 0.12)
            }

            ColumnLayout {
                id: reportCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: Theme.sp5
                spacing: Theme.sp3

                Rectangle {
                    implicitHeight: 22
                    implicitWidth: pillText.implicitWidth + Theme.sp3 * 2
                    radius: Theme.rFull
                    color: Theme.glass
                    border.color: Theme.glassBorder
                    border.width: 1
                    Text {
                        id: pillText
                        anchors.centerIn: parent
                        text: "AI 日报"
                        color: Theme.text
                        font.pixelSize: Theme.fsCaption
                        font.bold: true
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: page.timeGreeting() + "，" + page.userName() + "。"
                    color: Theme.text
                    font.pixelSize: Theme.fsPage
                    font.bold: true
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.glassBorder }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp3
                    visible: page.rv("usageMinutes", 0) > 0
                    Text { text: "💻"; font.pixelSize: Theme.fsTitle; Layout.preferredWidth: 24 }
                    Text { text: "使用电脑"; color: Theme.text; font.pixelSize: Theme.fsBody; Layout.fillWidth: true }
                    Text { text: page.fmtDuration(page.rv("usageMinutes", 0)); color: Theme.text; font.pixelSize: Theme.fsBody; font.bold: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp3
                    visible: page.rv("topApp", "").length > 0
                    Text { text: "🎮"; font.pixelSize: Theme.fsTitle; Layout.preferredWidth: 24 }
                    Text { text: page.rv("topApp", ""); color: Theme.text; font.pixelSize: Theme.fsBody; Layout.fillWidth: true; elide: Text.ElideRight }
                    Text { text: page.fmtDuration(page.rv("topAppMinutes", 0)); color: Theme.text; font.pixelSize: Theme.fsBody; font.bold: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp3
                    visible: page.rv("workMinutes", 0) > 0
                    Text { text: "💼"; font.pixelSize: Theme.fsTitle; Layout.preferredWidth: 24 }
                    Text { text: "工作 / 学习"; color: Theme.text; font.pixelSize: Theme.fsBody; Layout.fillWidth: true }
                    Text { text: page.fmtDuration(page.rv("workMinutes", 0)); color: Theme.text; font.pixelSize: Theme.fsBody; font.bold: true }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.sp3
                    visible: page.rv("shutdown", "").length > 0
                    Text { text: "🌙"; font.pixelSize: Theme.fsTitle; Layout.preferredWidth: 24 }
                    Text { text: page.rv("shutdown", ""); color: Theme.text; font.pixelSize: Theme.fsBody; Layout.fillWidth: true; elide: Text.ElideRight }
                }

                Item { Layout.fillHeight: true }

                Text { text: "今日建议"; color: Theme.textDim; font.pixelSize: Theme.fsSmall; font.bold: true }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: adviceText.implicitHeight + Theme.sp3 * 2
                    radius: Theme.rMd
                    color: Qt.rgba(1, 1, 1, 0.04)
                    border.color: Theme.glassBorder
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.sp3
                        spacing: Theme.sp3
                        Rectangle { Layout.preferredWidth: 3; Layout.fillHeight: true; radius: 1.5; color: Theme.sectionBar }
                        Text {
                            id: adviceText
                            Layout.fillWidth: true
                            text: "\u201C" + (page.advice.length > 0 ? page.advice : page.rv("advice", "")) + "\u201D"
                            color: Theme.text
                            font.pixelSize: Theme.fsBody
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        // ================= BOTTOM: 更新公告 + GitHub 热门 =================
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 168
            Layout.maximumHeight: 168
            spacing: Theme.sp4

            // 更新公告
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.rXl
                color: Theme.cardFill
                border.color: Theme.glassBorder
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp4
                    spacing: Theme.sp1
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "更新公告"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: updateService.updateAvailable ? "更新 ›" : "›"
                            color: updateService.updateAvailable ? Theme.ok : Theme.textDim
                            font.pixelSize: Theme.fsDefault
                            font.bold: updateService.updateAvailable
                        }
                    }
                    Text {
                        text: "当前 v" + updateService.currentVersion() + " · " + page.releaseStatus()
                        color: Theme.textMuted
                        font.pixelSize: Theme.fsSmall
                    }
                    Repeater {
                        model: page.releaseLines
                        delegate: Text {
                            Layout.fillWidth: true
                            text: "· " + modelData
                            color: Theme.textDim
                            font.pixelSize: Theme.fsSmall
                            elide: Text.ElideRight
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: page.releaseLines.length === 0
                        text: page.releaseLoaded ? "暂无更新内容（点击卡片查看发布页）" : "正在获取更新公告…"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fsSmall
                    }
                    Item { Layout.fillHeight: true }
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    // jump to Settings → 维护/更新 (it owns the progress bar)
                    onClicked: page.navigate(4)
                }
            }

            // GitHub 今日热门
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: Theme.rXl
                color: Theme.cardFill
                border.color: Theme.glassBorder
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.sp4
                    spacing: Theme.sp1
                    Text { text: "GitHub 今日热门"; color: Theme.text; font.pixelSize: Theme.fsDefault; font.bold: true }
                    Repeater {
                        model: page.trending
                        delegate: RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.sp2
                            Text {
                                text: (index + 1) + "."
                                color: Theme.textMuted
                                font.pixelSize: Theme.fsSmall
                                Layout.preferredWidth: 16
                            }
                            Text {
                                Layout.fillWidth: true
                                text: modelData.name
                                color: Theme.text
                                font.pixelSize: Theme.fsSmall
                                elide: Text.ElideMiddle
                            }
                            Text {
                                text: "★ " + page.starFmt(modelData.stars)
                                color: Theme.textDim
                                font.pixelSize: Theme.fsSmall
                            }
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: page.openUrl(modelData.url)
                            }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: page.trending.length === 0
                        text: page.trendingLoaded ? "暂时获取不到（GitHub 限流或网络问题）" : "正在获取今日热门…"
                        color: Theme.textMuted
                        font.pixelSize: Theme.fsSmall
                    }
                    Item { Layout.fillHeight: true }
                }
            }
        }
    }
}
