// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>

// Auto-update against a GitHub release (public repo).
// Flow: checkForUpdates() -> latest release tag & exe/zip asset URL;
//       downloadAndInstall() -> resolve official SHA-256, download
//       (official first, mirrors only as fallback), verify checksum,
//       extract/launch installer, quit self.
// Security: nothing is ever executed unless the downloaded file matches the
// official checksum fetched from GitHub itself.
class UpdateService : public QObject
{
    Q_OBJECT
public:
    explicit UpdateService(QObject *parent = nullptr);

    Q_INVOKABLE void checkForUpdates();          // async check against GitHub releases/latest
    Q_INVOKABLE void downloadAndInstall();       // download + verify + run installer, then quit
    Q_INVOKABLE QString currentVersion();        // local version "2.1.0"

    Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateAvailableChanged)
    Q_PROPERTY(QString downloadUrl READ downloadUrl NOTIFY updateAvailableChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY downloadStateChanged)
    Q_PROPERTY(int downloadProgress READ downloadProgress NOTIFY downloadStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY checkFinished)

    bool updateAvailable() const { return m_available; }
    QString latestVersion() const { return m_latest; }
    QString downloadUrl() const { return m_url; }
    bool downloading() const { return m_downloading; }
    int downloadProgress() const { return m_progress; }
    QString lastError() const { return m_lastError; }

signals:
    void updateAvailableChanged();
    void checkFinished(bool available);
    void downloadFinished(bool ok, QString message);
    void downloadStateChanged();

private:
    static bool versionGreater(const QString &remote, const QString &local);
    void parseLatestRelease(const QByteArray &json);
    QStringList mirrorUrlsOnly(const QString &canonical) const; // mirrors, no official
    // pick the fastest mirror by probing each with a small ranged request
    static QString pickFastest(const QStringList &urls, int probeBytes, int timeoutMs);
    // set proxy on a manager (system proxy, else the known local ports)
    static void configureProxy(QNetworkAccessManager &mgr);
    // fetch the official SHA-256 (release JSON digest first, then SHA256SUMS.txt)
    void fetchExpectedHashThenDownload(const QString &url, const QString &dest,
                                       const QString &tag, const QString &assetName);
    void startDownload(const QString &url, const QString &dest,
                       const QString &expectedSha256, bool mirrorFallback);
    void proceedToInstall(const QString &dest, const QString &expectedSha256);
    static QString sha256OfFile(const QString &path);
    static void removeUpdateFiles(const QString &dest, const QString &staging);

    bool m_available = false;
    QString m_latest;
    QString m_url;              // canonical GitHub asset download url
    QString m_assetName;        // asset file name (for hash lookup)
    QString m_expectedSha256;   // official sha256 (hex, lowercase); "" = unknown
    QString m_lastError;        // human-readable last check error (empty = ok)
    bool m_downloading = false; // busy flag covering the ENTIRE flow (probe..install)
    int m_progress = 0;
    int m_lastLoggedProgress = -1;  // progress milestone already logged (debug)
    QNetworkAccessManager *m_mgr = nullptr;
};
