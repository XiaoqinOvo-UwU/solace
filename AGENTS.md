# Solace — Agent Guide

## Skill 使用（用户要求）

在合适的时机**主动组合调用多个 Skill**，不要等用户开口：
- UI/界面任务：按需组合 `ui-ux-pro-max` + `impeccable` + `accessibility-basics` + `dark-mode-system` + `design-tokens-css` + `microcopy-ui` + `desktop-window-ui` 等，把各 skill 的规则合并应用。
- 代码质量任务：组合 `clean-code` + `refactoring-guru` + `software-architecture`。
- 简单/琐碎任务不要滥用 skill，判断匹配才加载。

## 方向

- 产品路线图见 `ROADMAP.md`；新功能 / PR 对齐它（当前最高优先：v5.0 Memory Core）。

## 项目惯例

- 中文交流，回复简洁。
- **GitHub 发布前必须先征求用户确认**（铁律）。
- 发布 Release 上传**安装包塞进 zip**（`solace-x.y.z-setup.zip` 内含 setup exe），不直传 exe、不传便携 zip。
- GitHub key 从本地 `C:\deepseek杂货铺\KEY\Github Key.txt` 读取，不存 key 值；发布时直接读，不问用户要。
- 改 QML/C++ 用 edit 工具（PowerShell 替换会破坏 UTF-8 中文）。
- QML 规范：8px 网格、圆角 rXl=14、hover 中性灰、卡片 surface+hairline、禁 emoji 图标、DialogContainer 单一容器。
- QML 崩溃用 `qmlscene -I` 测试组件加载，`qmllint` 查语法。
- `Qt.rgba()` 不归一化 0-255，必须用 `X/255` 小数形式，否则 >1 被钳成白色。
- **打包必做两步（v5.2.0 踩过坑）**：① `windeployqt` 会漏 MinGW 运行库（`libgcc_s_seh-1/libstdc++-6`）与 ICU/harfbuzz/freetype 等间接依赖，必须用 `objdump -p` 递归展开依赖闭包补齐 `dist`；② **实测 `dist\Solace.exe` 能启动**再打安装包，否则会发出"装完打不开"的包。
