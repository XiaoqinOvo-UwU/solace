# Solace

一个 Windows 桌面 AI 陪伴小工具：**一个会长期记住你、按你的节奏陪着你的 AI 伙伴**，外加网络、系统与娱乐等实用面板。

界面为 Qt Quick / QML 深色设计，支持自定义壁纸与玻璃模式，内置自动更新。

## 功能

- **AI 陪伴**：结构化角色卡（人设 / 场景 / 示例对话 / 开场白），让回复更像"人"而不是模板；只记真实发生过的事，不编造。
- **多 AI 联系人**：可添加任意多个 AI，支持**置顶**；每个 AI 拥有**独立的人设、头像与记忆**，并各自保存**两套内部提示词**（陪聊真人 / 个人助理），随时切换当前生效的那一套。
- **长期记忆**：分层记忆（用户事实 / 系统数据 / 事件 / 摘要）带重要性权重；"未完成话题"、兴趣偏好与关系状态都随 AI 独立保存。
- **本机检测（只读）**：网络（延迟 / DNS / 系统代理）、系统（内存 / 磁盘 / CPU / 开机时长）、占用内存最高的进程、以及应用自检 —— 全部只读，不改动你的系统。
- **状态感知**：读取前台应用、时间段与"不打扰"规则，决定要不要主动开口，而不是定时打扰。
- **首页日报**：今日使用时长 / 专注 / 关机时间 + AI 生成的一句建议；更新公告与 GitHub 更新卡片。
- **网络工具**：订阅导入 / 手动节点、延迟与测速、连通性自检。
- **娱乐**：运势、心情记录与**使用报告**。
- **外观**：深色 / 壁纸玻璃模式、自定义壁纸与模糊半径、动效提示。
- **自动更新**：每小时静默检查新版本，一键下载安装。

## 截图

![Solace 界面](docs/screenshots/app.png)

## 构建

**依赖**

- Qt 6（模块：Core, Gui, Quick, Qml, Widgets, Network, Svg, QuickControls2, Concurrent）
- CMake ≥ 3.21
- 支持 C++17 的编译器（Windows 推荐 MSYS2 / MinGW-w64）

**编译**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="<你的 Qt 路径>"
cmake --build build
```

**打包（Windows）**

```powershell
windeployqt --release --qmldir qml --dir dist build/Solace.exe
# 注意：windeployqt 可能漏掉 MinGW 运行库与 ICU/harfbuzz 等间接依赖。
# 发布前务必用 objdump 递归补齐 dist 缺失的 DLL，并实测 dist\Solace.exe 能启动。
# 然后用 Inno Setup 编译 installer/setup.iss，产物为 setup.exe（只把安装器打进 zip 上传）。
```

## 使用

1. 安装并启动，进入 **设置 → AI 配置** 填入自己的 API Key（DeepSeek 或任意 OpenAI 兼容接口），并选择模型。
2. 侧栏 **AI 联系人** 处点 **＋** 添加 AI；点卡片进入对话。每个 AI 的人设、头像与内部提示词都在 **聊天页左上角的 AI 头像 → AI 资料** 里单独设置。
3. 想让它顺手看看机器状态，直接问"网络卡不卡""后台谁在占内存""自检一下"即可（只读检测）。

> API Key 只保存在本机 `%APPDATA%\XiaoQin\XiaoQinTools`，用于旧版兼容目录未变；配置导出走 DPAPI 加密，不会以明文落盘。

## 下载

见 [Releases](https://github.com/XiaoqinOvo-UwU/solace/releases)。请使用应用内自动更新，或下载 release 中的 `solace-*-setup.zip` 手动安装（**不要**手动覆盖 `Program Files`）。

## 更多文档

使用 / 架构 / 贡献说明见 [项目 Wiki](https://github.com/XiaoqinOvo-UwU/solace/wiki)。

## 许可证

本项目基于 **GNU General Public License v3.0** 发布，见 [LICENSE](LICENSE)。

```
Solace  Copyright (C) 2026  The Solace Authors
```

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
