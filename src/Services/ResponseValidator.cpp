// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "ResponseValidator.h"
#include "FactFilter.h"
#include "ResponseRepair.h"
#include <QRegularExpression>

// final safety net: nothing that starts a <| control block may reach the UI.
// If one survives all earlier stages, drop the WHOLE block (up to the closing
// "|>", or the rest of the line when the closing marker is missing).
static QString dropResidualControlBlocks(const QString &s)
{
    QString out = s;
    int idx;
    while ((idx = out.indexOf(QStringLiteral("<|"))) >= 0) {
        const int end = out.indexOf(QStringLiteral("|>"), idx + 2);
        if (end >= 0) {
            out = out.left(idx) + out.mid(end + 2);
        } else {
            const int nl = out.indexOf('\n', idx);
            out = nl >= 0 ? (out.left(idx) + out.mid(nl)) : out.left(idx);
        }
    }
    return out.trimmed();
}

// =====================================================================
// v4.3 outbound guards — the reply is scanned BEFORE it reaches the UI.
// =====================================================================

// 1) guilt / pressure lines (structural dependency language). Users in
//    vulnerable states are the most susceptible; these are hard-dropped,
//    not softened by prompt alone. "你不理我我会难过" style lines never ship.
QString ResponseValidator::stripGuiltPressure(const QString &s, int *countOut)
{
    static const QStringList guiltLines = {
        "你不理我我会难过", "你不理我我会伤心", "你不在我会难过", "你不在我会想你",
        "你不陪我我会", "你走了我会", "你丢下我", "你不回来我会",
        "我会一直等你", "我就一直等你", "你不要离开我",
        "你两天没找我", "你三天没找我", "你很久没找我了",
        "你不在的时候我", "我一个人好孤单", "只有我一个人",
    };
    QString out = s;
    int n = 0;
    for (const QString &g : guiltLines) {
        int idx;
        while ((idx = out.indexOf(g)) >= 0) {
            // drop the whole line containing the phrase
            const int ls = out.lastIndexOf('\n', idx - 1) + 1;
            const int le = out.indexOf('\n', idx);
            const int end = le < 0 ? out.size() : le;
            out = out.left(ls) + out.mid(end);
            n++;
        }
    }
    if (countOut) *countOut = n;
    return out.trimmed();
}

// 2) persona-drift phrases that break the illusion — the AI identifies
//    itself as a generic assistant instead of the companion. Neutralized
//    in place rather than dropped (keeps the sentence's intent).
QString ResponseValidator::neutralizeDrift(const QString &s, int *countOut)
{
    static const QList<QPair<QString, QString>> driftMap = {
        { "作为一个AI", "我" },
        { "作为一个人工智能", "我" },
        { "作为一个语言模型", "我" },
        { "我是AI助手", "我是" },
        { "我是人工智能助手", "我" },
        { "我是语言模型", "我是" },
        { "AI助手", "我" },
    };
    QString out = s;
    int n = 0;
    for (const auto &p : driftMap) {
        int idx;
        while ((idx = out.indexOf(p.first)) >= 0) {
            out.replace(idx, p.first.size(), p.second);
            n++;
        }
    }
    if (countOut) *countOut = n;
    return out;
}

QString ResponseValidator::stripControlTokens(const QString &raw)
{
    // known control tokens with an OPTIONAL payload:
    //   <|ACT|> , <|ACT {json}|> , <|THINK|> , <|THINK {json}|> ,
    //   <|DELAY|> , <|DELAY 1.5|>
    static const QRegularExpression ctrlRe(
        "<\\|\\s*(ACT|THINK|DELAY)(?:\\s*\\{.*?\\}|\\s+[0-9.]+)?\\s*\\|>",
        QRegularExpression::DotMatchesEverythingOption);
    // legacy reasoning block <think>...</think>
    static const QRegularExpression thinkRe("<think>(.*?)</think>",
                                            QRegularExpression::DotMatchesEverythingOption);
    QString s = raw;
    s.remove(ctrlRe);
    s.remove(thinkRe);
    return dropResidualControlBlocks(s);
}

QString ResponseValidator::stripStageDirections(const QString &raw)
{
    QString s = raw;
    // full-width parens as literal chars (source is UTF-8; QRegularExpression
    // does NOT understand \uXXXX escapes — use \x{...} or raw chars)
    static const QRegularExpression parenRe(QStringLiteral("[（(][^（()]*[)）]"));
    static const QRegularExpression starRe(QStringLiteral("\\*[^*\\n]*\\*"));
    s.remove(parenRe);
    s.remove(starRe);
    // NOTE: no length-based line filtering here — natural replies are
    // unquoted and can be long; line-level narration filtering happens in
    // trimToDialog with a strict narration pattern.
    return s.trimmed();
}

// repair (not delete) fabricated observations:
//   - line has an observation phrase
//   - AND no verified fact text appears on the line
// -> rewrite via ResponseRepair; fall back to deletion when no rewrite exists
QString ResponseValidator::stripUnsupportedObservations(const QString &raw, const QString &factText)
{
    return stripUnsupportedObservationsImpl(raw, factText, nullptr, nullptr);
}

QString ResponseValidator::stripUnsupportedObservationsImpl(const QString &raw, const QString &factText,
                                                            int *repaired, int *dropped)
{
    const QStringList lines = raw.split('\n');
    QStringList kept;
    int nRepaired = 0, nDropped = 0;
    for (const QString &line : lines) {
        bool unsupported = false;
        for (const QString &ph : FactFilter::observationPhrases()) {
            if (line.contains(ph)) {
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
                if (!supported) { unsupported = true; break; }
            }
        }
        if (!unsupported) {
            kept << line;
            continue;
        }
        // try to repair into a natural uncertain expression first
        QString r = ResponseRepair::repairObservationLine(line);
        if (!r.isEmpty()) {
            kept << r;
            ++nRepaired;
        } else {
            ++nDropped; // last-resort deletion
        }
    }
    if (repaired) *repaired = nRepaired;
    if (dropped) *dropped = nDropped;
    return kept.join('\n').trimmed();
}

// keep dialog lines; drop only obvious standalone narration lines.
// Natural replies are UNQUOTED ("没关系啦") and can be long, so we must not
// drop long lines just because they lack quotes — that would silently kill
// replies (and idle chat entirely). We only drop lines that read like
// narration: verb-led, quote-less, and ending with 。.
QString ResponseValidator::trimToDialog(const QString &raw)
{
    static const QRegularExpression narrRe(QStringLiteral(
        "^(我|他|她|它|你|大家|房间|空气|气氛|周围)[^，。！？]{2,20}"
        "(了|着|下|起|在|又|也|便|就|开始|继续|轻轻|默默|缓缓|低头|抬头|转身|露出|看着|听到|感到|觉得|想了|沉默|停顿|叹气|微笑|点头|摇头)"
        "[^\"“”]*。$"));
    QStringList kept;
    for (const QString &line : raw.split('\n')) {
        QString l = line.trimmed();
        if (l.isEmpty()) continue;
        bool hasQuote = l.contains(QString("\"")) || l.contains(QString("“")) || l.contains(QString("”"));
        if (hasQuote) { kept << l; continue; }
        // drop only high-confidence narration lines
        if (l.length() > 8 && l.length() <= 60 && narrRe.match(l).hasMatch())
            continue;
        kept << l;
    }
    return kept.join('\n').trimmed();
}

// drop lines where the AI describes a physical action it cannot do
// (抱抱/点外卖/走到你身边...). Keeps the AI in online/network mode.
QString ResponseValidator::stripPhysicalActions(const QString &raw)
{
    const QStringList lines = raw.split('\n');
    QStringList kept;
    for (const QString &line : lines) {
        bool bad = false;
        for (const QString &ph : FactFilter::physicalActionPhrases()) {
            if (line.contains(ph)) { bad = true; break; }
        }
        if (!bad) kept << line;
    }
    return kept.join('\n').trimmed();
}

ResponseValidator::Result ResponseValidator::validate(const QString &raw, const QString &verifiedFacts)
{
    Result r;
    QString s = raw;
    const QString before = s;

    s = stripControlTokens(s);
    s = stripStageDirections(s);

    // observation fabrication check (uses the fact block as support base).
    // Unsupported observations are REPAIRED, not just deleted.
    int repaired = 0, dropped = 0;
    const QString withoutObs = stripUnsupportedObservationsImpl(s, verifiedFacts, &repaired, &dropped);
    r.observationsRepaired = repaired;
    if (dropped > 0)
        r.observationsStripped = dropped;
    s = withoutObs;

    // online-mode: never let the AI describe physical body actions
    s = stripPhysicalActions(s);

    // ---- v4.3 outbound guards ----
    s = stripGuiltPressure(s, &r.guiltStripped);
    s = neutralizeDrift(s, &r.driftRepaired);

    s = trimToDialog(s);
    s = s.trimmed();
    // final safety filter: no control token may reach the UI
    s = dropResidualControlBlocks(s);

    r.text = s;
    r.changed = (s != before);
    return r;
}
