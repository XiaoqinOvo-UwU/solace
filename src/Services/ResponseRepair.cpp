// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "ResponseRepair.h"
#include "FactFilter.h"

bool ResponseRepair::hasUnsupportedObservation(const QString &line)
{
    for (const QString &ph : FactFilter::observationPhrases())
        if (line.contains(ph)) return true;
    return false;
}

QString ResponseRepair::repairObservationLine(const QString &line)
{
    if (!hasUnsupportedObservation(line)) return QString();

    // pick the gentlest, most plausible hedge based on what the line mentions.
    // Every rewrite is uncertain ("感觉/好像/听说"), never an assertion.
    if (line.contains("熬夜") || line.contains("晚睡") || line.contains("没睡")
        || line.contains("失眠") || line.contains("几点睡"))
        return QStringLiteral("感觉你最近好像挺晚睡的，要注意休息哦。");

    if (line.contains("游戏") || line.contains("在玩") || line.contains("apex")
        || line.contains("minecraft") || line.contains("我的世界")
        || line.contains("打") || line.contains("电竞"))
        return QStringLiteral("听说你最近有在玩游戏？偶尔放松一下也不错~");

    if (line.contains("忙") || line.contains("工作") || line.contains("代码")
        || line.contains("写") || line.contains("加班") || line.contains("项目"))
        return QStringLiteral("感觉你最近好像挺忙的，别太累啦。");

    if (line.contains("累") || line.contains("困") || line.contains("疲惫"))
        return QStringLiteral("感觉你最近有点累的样子，记得照顾好自己。");

    if (line.contains("难过") || line.contains("不开心") || line.contains("心情")
        || line.contains("低落") || line.contains("烦"))
        return QStringLiteral("感觉你最近心情可能不太好，想聊的话我一直都在。");

    // generic fallback — still hedged and caring
    return QStringLiteral("感觉你最近好像挺忙的，注意休息哦。");
}
