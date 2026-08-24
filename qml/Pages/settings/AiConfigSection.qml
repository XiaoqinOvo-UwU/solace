import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../Components"

// AI 配置：OpenAI-compatible endpoint (base url / model / key) + quick presets.
// RULE: a field the user has edited is NEVER overwritten by a preset — presets
// only fill fields that are still untouched. Model uses the same themed input
// as the other fields, with a dropdown menu listing models detected via the key.
// Emits notify(message) so the hosting page can surface inline feedback.
SettingsSectionCard {
    id: section

    signal notify(string message)

    title: "AI 配置"

    property var modelList: []

    Text {
        Layout.fillWidth: true
        text: "支持任意 OpenAI 兼容的 API 接入（DeepSeek / Claude / GPT / 自建中转）"
        color: Theme.textDim
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }

    Text { text: "Base URL"; color: Theme.textDim; font.pixelSize: 12; Layout.topMargin: 4 }
    ThemedTextField {
        id: editBaseUrl
        placeholderText: "https://api.deepseek.com/v1"
        text: aiService.apiBaseUrl()
    }

    Text { text: "Model"; color: Theme.textDim; font.pixelSize: 12 }
    RowLayout {
        Layout.fillWidth: true
        spacing: 8
        ThemedTextField {
            id: editModel
            Layout.fillWidth: true
            placeholderText: "deepseek-chat"
            text: aiService.apiModel()
        }
        AppButton {
            text: "检测模型"
            variant: "ghost"
            implicitHeight: 36
            onClicked: section.detectModels()
        }
        AppButton {
            text: "选择"
            variant: "ghost"
            implicitHeight: 36
            enabled: section.modelList.length > 0
            onClicked: modelMenu.open()
        }
    }

    // dropdown of detected models — same dark visual language as the inputs
    Menu {
        id: modelMenu
        y: -8
        width: Math.max(200, editModel.width)
        Instantiator {
            model: section.modelList
            delegate: MenuItem {
                text: modelData
                height: 34
                onTriggered: {
                    editModel.text = modelData
                    section.saveConfig()   // apply immediately, like presets
                }
            }
            onObjectAdded: (index, object) => modelMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => modelMenu.removeItem(object)
        }
    }

    Text { text: "API Key"; color: Theme.textDim; font.pixelSize: 12 }
    ThemedTextField {
        id: editApiKey
        placeholderText: "sk-..."
        text: aiService.apiKey()
        echoMode: TextInput.Password
    }

    // Detect models supported by the current key/endpoint via GET {base}/models.
    function detectModels() {
        var url = editBaseUrl.text.trim()
        var key = editApiKey.text.trim()
        if (url.length === 0 || key.length === 0) {
            appCore.showToast("请先填写 Base URL 和 API Key")
            return
        }
        appCore.showToast("正在检测可用模型…")
        detectTimer.restart()
    }

    Timer {
        id: detectTimer
        interval: 60          // let the toast paint before the blocking call
        onTriggered: {
            var url = editBaseUrl.text.trim()
            var key = editApiKey.text.trim()
            var list = aiService.fetchAvailableModels(url, key)
            section.modelList = list
            if (list.length === 0) {
                appCore.showToast("未检测到模型（接口可能不支持 /models），可手动输入模型名")
                return
            }
            appCore.showToast("检测到 " + list.length + " 个模型，可点「选择」查看")
        }
    }

    // ---- provider selector ----
    Text {
        text: "接口预设"
        color: Theme.text
        font.pixelSize: 13
        font.bold: true
        Layout.topMargin: 8
    }
    Text {
        Layout.fillWidth: true
        text: "DeepSeek / OpenAI：点击立即切换生效。自定义：恢复你自己的配置（预设切换不会影响它）。中转站地址请带 /v1。"
        color: Theme.textDim
        font.pixelSize: 11
        wrapMode: Text.Wrap
    }

    // Persist fields to the ACTIVE config only. Used by presets — switching
    // providers must NEVER touch the dedicated custom slot.
    function saveActiveConfig() {
        var url = editBaseUrl.text.trim()
        var key = editApiKey.text.trim()
        var model = editModel.text.trim()
        if (url.length === 0 || model.length === 0) {
            appCore.showToast("Base URL 和模型名不能为空，请填写后再保存")
            return false
        }
        aiService.setApiBaseUrl(url)
        aiService.setApiModel(model)
        aiService.setApiKey(key)
        if (key.length > 0 && aiService.rememberApiKeyFor)
            aiService.rememberApiKeyFor(url, key)
        section.notify("AI 配置已保存~")
        return true
    }

    // Manual save: persists to the ACTIVE config AND the custom slot —
    // hand-edited fields are by definition the user's own setup.
    function saveConfig() {
        var url = editBaseUrl.text.trim()
        var key = editApiKey.text.trim()
        var model = editModel.text.trim()
        if (url.length === 0 || model.length === 0) {
            appCore.showToast("Base URL 和模型名不能为空，请填写后再保存")
            return false
        }
        aiService.setApiBaseUrl(url)
        aiService.setApiModel(model)
        aiService.setApiKey(key)
        aiService.setCustomApi(url, model, key)
        if (key.length > 0 && aiService.rememberApiKeyFor)
            aiService.rememberApiKeyFor(url, key)
        section.notify("AI 配置已保存~")
        appCore.showToast("AI 配置已保存~")
        return true
    }

    // Preset = switch the ACTIVE config now (AI uses it on the next message).
    // The custom slot is never touched — 自定义 restores it afterwards.
    function applyPreset(presetUrl, presetModel, presetLabel) {
        editBaseUrl.text = presetUrl
        editModel.text = presetModel
        if (aiService.apiKeyFor) {
            var k = aiService.apiKeyFor(presetUrl)
            if (k.length > 0) editApiKey.text = k
        }
        if (section.saveActiveConfig())
            appCore.showToast("已切换并保存为 " + presetLabel)
    }

    function clearCustom() {
        // restore the DEDICATED custom slot — switching to presets and back
        // always lands on the user's own config, never the preset's
        editBaseUrl.text = aiService.customBaseUrl()
        editModel.text = aiService.customModel()
        editApiKey.text = aiService.customApiKey()
        appCore.showToast("已恢复自定义配置")
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 3
        rowSpacing: 8
        columnSpacing: 8

        AppButton {
            text: "DeepSeek"
            variant: "ghost"
            Layout.fillWidth: true
            implicitHeight: 32
            onClicked: section.applyPreset("https://api.deepseek.com/v1", "deepseek-chat", "DeepSeek")
        }
        AppButton {
            text: "OpenAI"
            variant: "ghost"
            Layout.fillWidth: true
            implicitHeight: 32
            onClicked: section.applyPreset("https://api.openai.com/v1", "gpt-4o", "OpenAI")
        }
        AppButton {
            text: "自定义"
            variant: "ghost"
            Layout.fillWidth: true
            implicitHeight: 32
            onClicked: section.clearCustom()
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        AppButton {
            text: "保存 AI 配置"
            Layout.fillWidth: true
            implicitHeight: 36
            onClicked: section.saveConfig()
        }
    }
}
