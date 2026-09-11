// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../../Components"

// 梯子路径：Clash Verge / v2rayN executable paths (auto-detected when empty).
SettingsSectionCard {
    id: section

    title: "梯子路径"

    Text { text: "Clash Verge"; color: Theme.textDim; font.pixelSize: 12 }
    ThemedTextField {
        padding: 8
        placeholderText: "自动检测"
        text: proxyService.clashExe()
    }
    Text { text: "v2rayN"; color: Theme.textDim; font.pixelSize: 12 }
    ThemedTextField {
        padding: 8
        placeholderText: "自动检测"
        text: proxyService.v2rayExe()
    }
}
