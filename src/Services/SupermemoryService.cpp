// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "SupermemoryService.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>

namespace {

const char *kBase = "https://api.supermemory.ai";
const int kTimeoutMs = 8000;

QString apiKey()
{
    return qEnvironmentVariable("SUPERMEMORY_API_KEY").trimmed();
}

// POST JSON and return the raw response body; empty on timeout/error.
QString postJson(const QString &path, const QJsonObject &body, int timeoutMs)
{
    if (apiKey().isEmpty())
        return QString();

    QNetworkAccessManager mgr;
    QNetworkRequest req;
    req.setUrl(QUrl(QString::fromLatin1(kBase) + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", ("Bearer " + apiKey()).toUtf8());
    QNetworkReply *reply = mgr.post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    QEventLoop loop;
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QTimer::singleShot(timeoutMs, &loop, [&loop, &timedOut]() { timedOut = true; loop.quit(); });
    loop.exec();

    if (timedOut) {
        reply->abort();
        reply->deleteLater();
        return QString();
    }
    const QByteArray data = reply->readAll();
    reply->deleteLater();
    return QString::fromUtf8(data);
}

} // namespace

bool SupermemoryService::enabled()
{
    return !apiKey().isEmpty();
}

QString SupermemoryService::containerTag()
{
    QString t = qEnvironmentVariable("SUPERMEMORY_CONTAINER_TAG").trimmed();
    if (t.isEmpty())
        t = QStringLiteral("solace");
    t.remove(QRegularExpression(QStringLiteral("[^A-Za-z0-9_:-]")));
    if (t.isEmpty())
        t = QStringLiteral("solace");
    return t.left(100);
}

QString SupermemoryService::search(const QString &query, int limit)
{
    if (!enabled() || query.trimmed().isEmpty())
        return QString();

    QJsonObject include;
    include.insert("relatedMemories", true);

    QJsonObject body;
    body.insert("q", query.left(400));
    body.insert("containerTag", containerTag());
    body.insert("searchMode", "hybrid");
    body.insert("limit", qBound(1, limit, 20));
    body.insert("include", include);

    const QJsonObject resp =
        QJsonDocument::fromJson(postJson(QStringLiteral("/v4/search"), body, kTimeoutMs).toUtf8()).object();

    QStringList lines;
    const QJsonArray results = resp.value("results").toArray();
    for (const QJsonValue &v : results) {
        const QJsonObject r = v.toObject();
        QString text = r.value("memory").toString().trimmed();
        if (text.isEmpty())
            text = r.value("chunk").toString().trimmed();
        if (text.isEmpty())
            continue;
        text.replace(QRegularExpression(QStringLiteral("\\s+")), " ");
        lines << "- " + text.left(200);
        if (lines.size() >= limit)
            break;
    }
    return lines.join("\n");
}

void SupermemoryService::addMemory(const QString &content, const QString &customId)
{
    if (!enabled() || content.trimmed().isEmpty())
        return;

    QJsonObject body;
    body.insert("content", content.left(4000));
    body.insert("containerTag", containerTag());
    body.insert("taskType", "memory");
    if (!customId.isEmpty())
        body.insert("customId", customId);

    postJson(QStringLiteral("/v3/documents"), body, kTimeoutMs);
}
