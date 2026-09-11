// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "FactFilter.h"
#include <QRegularExpression>
#include <QStringList>

QList<Fact> FactFilter::facts(const QList<Fact> &all)
{
    QList<Fact> out;
    for (const Fact &f : all)
        if (f.isFact()) out.append(f);
    return out;
}

QList<Fact> FactFilter::hypotheses(const QList<Fact> &all)
{
    QList<Fact> out;
    for (const Fact &f : all)
        if (f.isHypothesis()) out.append(f);
    return out;
}

QList<Fact> FactFilter::clean(const QList<Fact> &all, double minConfidence)
{
    QList<Fact> out;
    for (const Fact &f : all) {
        if (f.rejected) continue;
        if (f.source == FactSource::UserMessage || f.source == FactSource::SystemData) {
            if (f.confidence >= minConfidence) out.append(f);
        } else {
            out.append(f); // memory/hypothesis keep their own semantics
        }
    }
    return out;
}

const QStringList &FactFilter::observationPhrases()
{
    static const QStringList phrases = {
        "我看到你", "我注意到你", "我发现你", "你刚刚一直", "你刚刚在", "你一直在",
        "你又在", "你总是", "你又去", "你是不是在", "你是不是又", "你正在", "你刚在",
        "你又在玩", "你又在打",
    };
    return phrases;
}

const QStringList &FactFilter::stagePhrases()
{
    static const QStringList phrases = {}; // stage actions handled by regex
    return phrases;
}

const QStringList &FactFilter::physicalActionPhrases()
{
    static const QStringList phrases = {
        "抱抱", "抱你", "抱住了", "摸摸头", "摸摸你的", "拍拍", "拍你",
        "点外卖", "帮你点", "我去点", "倒水", "给你倒", "买饭", "做饭",
        "煮了", "泡了杯", "端来", "递给你", "走到你", "来到你", "站在你",
        "坐在你", "在你身边", "坐到你旁边", "伸手", "敲门", "送到你家",
        "我帮你买", "我去买", "我给你倒", "把门打开", "开着门",
    };
    return phrases;
}

QString FactFilter::stripUnsupportedObservations(const QString &reply, const QString &factText)
{
    QString out = reply;
    // split into lines so we can drop a whole fabricated line
    const QStringList lines = reply.split('\n');
    QStringList kept;
    for (const QString &line : lines) {
        bool fabricated = false;
        for (const QString &ph : observationPhrases()) {
            if (line.contains(ph)) {
                // check whether any verified fact text also appears on this line
                bool supported = false;
                const QStringList facts = factText.split('\n');
                for (const QString &f : facts) {
                    QString cleanF = f;
                    cleanF.remove(QRegularExpression("^-\\s*\\[[^\\]]+\\]\\s*"));
                    if (!cleanF.isEmpty() && line.contains(cleanF.left(qMax(3, cleanF.size() / 2)))) {
                        supported = true;
                        break;
                    }
                }
                if (!supported) {
                    fabricated = true;
                    break;
                }
            }
        }
        if (!fabricated) kept << line;
    }
    return kept.join('\n').trimmed();
}
