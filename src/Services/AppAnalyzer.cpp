// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "AppAnalyzer.h"
#include <QFileInfo>
#include <QStringList>

namespace {

bool containsAny(const QString &s, const QStringList &words)
{
    for (const QString &w : words)
        if (s.contains(w)) return true;
    return false;
}

} // namespace

bool AppAnalyzer::isExcluded(const QString &exe, const QString &title)
{
    const QString e = exe.toLower();
    const QString t = title.toLower();
    static const QStringList needles = {
        // proxies / accelerators
        "clash", "v2ray", "nekoray", "nynya", "加速",
        // file explorer + system chrome
        "explorer", "资源管理器", "svchost", "dwm", "runtimebroker",
        "textinputhost", "searchapp", "applicationframehost", "lockapp",
        "sihost", "taskmgr", "任务管理器", "控制面板", "nvidia",
        "startmenuexperiencehost", "shellexperiencehost",
        // our own window + OS settings
        "小钦的工具", "设置",
        // game-store background clients (not the games themselves)
        "steam",
    };
    return containsAny(e, needles) || containsAny(t, needles);
}

QString AppAnalyzer::appLabel(const QString &exe, const QString &title)
{
    const QString e = exe.toLower();
    QString label = QFileInfo(exe).completeBaseName().trimmed();
    if (label.isEmpty()) label = title;
    if (e.contains("minecraft")) return "Minecraft";
    if (e.contains("r5apex") || e.contains("apex")) return "Apex 英雄";
    if (e.contains("code")) return "代码编辑器";
    if (e.contains("chrome") || e.contains("edge") || e.contains("firefox") || e.contains("opera"))
        return "浏览器";
    if (e.contains("wechat") || e.contains("qq") || e.contains("discord") || e.contains("telegram"))
        return "聊天软件";
    if (e.contains("cloudmusic") || e.contains("spotify") || e.contains("qqmusic"))
        return "音乐播放器";
    if (e.contains("wps") || e.contains("word") || e.contains("excel") || e.contains("powerpnt"))
        return "办公文档";
    if (e.contains("pwsh") || e.contains("powershell") || e.contains("cmd") || e.contains("mintty"))
        return "终端";
    return label;
}

AppAnalysis AppAnalyzer::analyze(const QString &exe, const QString &title, int minutes)
{
    Q_UNUSED(minutes);
    AppAnalysis a;
    a.source = "system_data";

    if (isExcluded(exe, title)) {
        a.isSystemNoise = true;
        a.category = "noise";
        return a;
    }

    const QString e = exe.toLower();
    const QString t = title.toLower();
    const QString label = appLabel(exe, title);

    // ---- gaming (highest confidence: window is a game client) ----
    if (containsAny(e, {"minecraft", "r5apex", "apex", "fortnite", "valorant",
                        "csgo", "cs2", "dota2", "league", "lol", "genshin",
                        "原神", "eldenring", "steamapps", "battle.net", "epicgames"})) {
        a.category = "gaming";
        a.content = "用户正在玩" + label;
        a.confidence = 0.95;
        return a;
    }
    // ---- creating / coding ----
    if (containsAny(e, {"code", "visualstudio", "clion", "pycharm", "intellij", "idea",
                        "eclipse", "sublime", "notepad", "vim", "godot", "unity",
                        "blender", "qtcreator", "androidstudio"})) {
        a.category = "creating";
        a.content = "用户可能正在编程/创作（" + label + "）";
        a.confidence = 0.80;
        return a;
    }
    // ---- browsing: NEVER infer the content ----
    if (containsAny(e, {"chrome", "msedge", "edge", "firefox", "opera", "360chrome"})) {
        a.category = "browsing";
        a.content = "用户正在使用浏览器（不推测浏览内容）";
        a.confidence = 0.80;
        return a;
    }
    // ---- chatting ----
    if (containsAny(e, {"wechat", "qq", "discord", "telegram", "feishu", "dingtalk",
                        "钉钉", "wework"})) {
        a.category = "chatting";
        a.content = "用户正在使用聊天软件（" + label + "）";
        a.confidence = 0.80;
        return a;
    }
    // ---- media ----
    if (containsAny(e, {"cloudmusic", "spotify", "qqmusic", "bilibili", "爱奇艺",
                        "youku", "mpv", "vlc", "potplayer"})) {
        a.category = "media";
        a.content = "用户可能在听音乐/看视频（" + label + "）";
        a.confidence = 0.70;
        return a;
    }
    // ---- document ----
    if (containsAny(e, {"word", "wps", "excel", "powerpnt", "wpspdf"})) {
        a.category = "document";
        a.content = "用户正在处理文档（" + label + "）";
        a.confidence = 0.80;
        return a;
    }
    // ---- terminal ----
    if (containsAny(e, {"pwsh", "powershell", "cmd", "git-bash", "gitbash",
                        "mintty", "wezterm", "alacritty", "windowsterminal"})) {
        a.category = "terminal";
        a.content = "用户正在使用终端（" + label + "）";
        a.confidence = 0.80;
        return a;
    }

    // ---- fallback: name the app, no behaviour guessing ----
    Q_UNUSED(t);
    a.category = "other";
    a.content = "用户正在使用 " + (label.isEmpty() ? title : label);
    a.confidence = 0.70;
    return a;
}
