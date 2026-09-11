// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "MemoryImportanceEvaluator.h"
#include <QStringList>

double MemoryImportanceEvaluator::importanceScore(const QString &text)
{
    const QString t = text.simplified();
    if (t.isEmpty()) return 0.0;
    const QString lower = t.toLower();

    // 1) explicit "remember this" — the strongest signal
    static const QStringList rememberMarks = {
        "记住", "记一下", "记着", "帮我记", "别忘了", "要记住", "记得啊",
        "记下来", "存一下", "你要记住",
    };
    for (const QString &m : rememberMarks)
        if (t.contains(m)) return 1.0;

    // 2) explicit preferences (like / dislike / love / hate)
    static const QStringList prefMarks = {
        "我喜欢", "我超爱", "我最爱", "特别爱", "真的爱", "我很喜欢",
        "我爱", "最爱", "我讨厌", "我不喜欢", "不喜欢", "讨厌", "最爱玩",
        "特别喜欢", "超喜欢", "钟爱",
    };
    for (const QString &m : prefMarks)
        if (t.contains(m)) return 0.92;

    // 3) habits / repeated behavior
    static const QStringList habitMarks = {
        "经常", "习惯", "每天晚上", "每天早上", "每天", "每次", "老是",
        "总喜欢", "日常", "平时", "作息", "一到", "闲着就", "一有空就",
        "常年", "隔三差五",
    };
    for (const QString &m : habitMarks)
        if (t.contains(m)) return 0.85;

    // 4) explicit updates / negations of a previously-known fact — needed for
    //    the conflict manager to deprecate the old memory (do not drop these)
    static const QStringList updateMarks = {
        "不玩", "不喝", "不再", "不打了", "不看了", "退坑", "卸载", "删了",
        "戒了", "放弃了", "现在不", "以后不", "早就不", "已经不喜欢",
    };
    for (const QString &m : updateMarks)
        if (t.contains(m)) return 0.6;

    // 5) important experiences / references to shared history
    static const QStringList expMarks = {
        "我们一起", "当时", "那天", "上次", "那次", "之前帮我", "帮过我",
        "解决了", "重要", "改变了我", "第一次", "值得纪念", "记不太清了",
    };
    for (const QString &m : expMarks)
        if (t.contains(m)) return 0.65;

    // 6) emotionally significant moments (weak but real)
    static const QStringList moodMarks = {
        "难过", "想哭", "压力", "崩溃", "焦虑", "孤独", "想被陪伴",
        "太开心", "太好了", "好累", "很累", "有点累", "好困", "好累啊",
        "疲惫", "失眠", "睡不着",
    };
    for (const QString &m : moodMarks)
        if (t.contains(m)) return 0.55;

    // 7) small talk / one-off questions / short acknowledgements
    static const QStringList noiseMarks = {
        "吃了吗", "在吗", "早上好", "中午好", "晚上好", "晚安", "早安",
        "再见", "拜拜", "哈哈", "hh", "嗯嗯", "嗯", "哦", "好", "哦哦",
        "没事", "随便", "不知道", "还行", "一般", "睡了吗",
    };
    for (const QString &m : noiseMarks)
        if (t == m || t.contains(m)) return 0.0;

    // question-heavy one-off messages are almost never long-term facts
    if ((t.contains("吗") || t.contains("？") || t.contains("?")) && t.size() < 30)
        return 0.15;

    // very short replies without any strong signal
    if (t.size() <= 6) return 0.05;

    // default: generic chat — borderline
    return 0.3;
}

ImportanceLevel MemoryImportanceEvaluator::level(const QString &text)
{
    const double s = importanceScore(text);
    if (s >= 0.8) return ImportanceLevel::High;
    if (s >= 0.5) return ImportanceLevel::Medium;
    if (s >= 0.25) return ImportanceLevel::Low;
    return ImportanceLevel::Noise;
}

bool MemoryImportanceEvaluator::worthStoring(const QString &text)
{
    return importanceScore(text) >= 0.5;
}

QString MemoryImportanceEvaluator::levelName(ImportanceLevel l)
{
    switch (l) {
    case ImportanceLevel::Noise:  return QStringLiteral("noise");
    case ImportanceLevel::Low:    return QStringLiteral("low");
    case ImportanceLevel::Medium: return QStringLiteral("medium");
    case ImportanceLevel::High:   return QStringLiteral("high");
    }
    return QStringLiteral("noise");
}
