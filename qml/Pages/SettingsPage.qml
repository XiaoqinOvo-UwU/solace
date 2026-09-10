import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../Components"
import "settings"

// Settings page: scrollable glass cards. Each functional group is a
// self-contained section component under settings/; this file only
// composes them and owns the shared inline note.
Page {
    id: root
    padding: 0
    background: Rectangle { color: "transparent" }

    property string note: ""

    // fixed header + scrollable body — same top slot as other pages
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        PageHeader {
            title: "设置"
            subtitle: "按你的习惯，把一切都调好"
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
                spacing: 14

                ProxySection {}

                AiConfigSection {
                    onNotify: (message) => root.note = message
                }

                WallpaperSection {
                    id: wallpaperSection
                }

                AppearanceSection {
                    wallpaperUrl: wallpaperSection.previewUrl
                }

                MaintenanceSection {
                    onNotify: (message) => root.note = message
                }

                Label {
                    Layout.fillWidth: true
                    color: "#4EA86B"
                    font.pixelSize: 13
                    text: root.note
                    wrapMode: Text.Wrap
                    visible: root.note.length > 0
                }

                Item { Layout.preferredHeight: 16 }

                Label {
                    Layout.fillWidth: true
                    horizontalAlignment: Text.AlignHCenter
                    text: "小钦的工具 v" + updateService.currentVersion() + " · 泉此方天下第一"
                    color: Theme.textDim
                    font.pixelSize: 12
                }
            }
        }
    }
}
