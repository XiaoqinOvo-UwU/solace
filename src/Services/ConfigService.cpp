// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The XiaoQinTools Authors
// This file is part of XiaoQinTools, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "ConfigService.h"

#include <QDir>
#include <QStandardPaths>
#include <QFile>
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QSettings>
#include <QByteArray>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {
const QLatin1String kDpapiPrefix("dpapi:v1:");
}

ConfigService &ConfigService::instance()
{
    static ConfigService inst;
    return inst;
}

// ---- DPAPI secret helpers ----
QString ConfigService::encryptSecret(const QString &plain)
{
#ifdef Q_OS_WIN
    if (plain.isEmpty()) return plain;
    QByteArray in = plain.toUtf8();
    DATA_BLOB inBlob, outBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(const_cast<char *>(in.constData()));
    inBlob.cbData = static_cast<DWORD>(in.size());
    outBlob.pbData = nullptr;
    outBlob.cbData = 0;
    if (CryptProtectData(&inBlob, L"XiaoQinToolsApiKey", nullptr, nullptr, nullptr,
                         CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        QByteArray cipher(reinterpret_cast<const char *>(outBlob.pbData),
                          static_cast<int>(outBlob.cbData));
        LocalFree(outBlob.pbData);
        return kDpapiPrefix + QString::fromLatin1(cipher.toBase64());
    }
    // DPAPI failure: NEVER fall back to writing plaintext to disk. Return empty
    // so the field persists as "" (fail-closed); the in-memory value stays
    // usable for this session and the next save retries encryption.
    qWarning("DPAPI CryptProtectData failed (error %lu) - secret not persisted",
             static_cast<unsigned long>(GetLastError()));
    return QString();
#else
    return plain;
#endif
}

QString ConfigService::decryptSecret(const QString &stored)
{
    if (stored.isEmpty()) return stored;
    if (!stored.startsWith(kDpapiPrefix)) return stored; // legacy plaintext
#ifdef Q_OS_WIN
    QByteArray cipher = QByteArray::fromBase64(stored.mid(kDpapiPrefix.size()).toLatin1());
    DATA_BLOB inBlob, outBlob;
    inBlob.pbData = reinterpret_cast<BYTE *>(cipher.data());
    inBlob.cbData = static_cast<DWORD>(cipher.size());
    outBlob.pbData = nullptr;
    outBlob.cbData = 0;
    if (CryptUnprotectData(&inBlob, nullptr, nullptr, nullptr, nullptr,
                           CRYPTPROTECT_UI_FORBIDDEN, &outBlob)) {
        QByteArray plain(reinterpret_cast<const char *>(outBlob.pbData),
                         static_cast<int>(outBlob.cbData));
        LocalFree(outBlob.pbData);
        return QString::fromUtf8(plain);
    }
    // Decrypt failed (e.g. different user profile). Return EMPTY — never hand
    // the ciphertext back as a usable key: a non-empty "dpapi:v1:..." value
    // would bypass callers' empty-guards and leak the blob to the network.
    qWarning("DPAPI CryptUnprotectData failed (error %lu) - secret unavailable",
             static_cast<unsigned long>(GetLastError()));
    return QString();
#else
    return stored;
#endif
}

QString ConfigService::configDir() const
{
    // Keep the "杂货铺" concept alive: settings live under AppData\XiaoQinTools
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir d(base);
    if (!d.exists()) d.mkpath(".");
    return base;
}

QString ConfigService::configPath() const
{
    return configDir() + "/config.json";
}

void ConfigService::load()
{
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) return;
    QJsonObject o = doc.object();
    if (o.contains("base_url")) m_baseUrl = o.value("base_url").toString();
    if (o.contains("model")) m_model = o.value("model").toString();
    if (o.contains("api_key")) m_apiKey = decryptSecret(o.value("api_key").toString());
    if (o.contains("api_keys")) {
        QJsonObject raw = o.value("api_keys").toObject();
        QJsonObject dec;
        for (auto it = raw.begin(); it != raw.end(); ++it)
            dec.insert(it.key(), decryptSecret(it.value().toString()));
        m_apiKeys = dec;
    }
    if (o.contains("custom_base_url")) m_customBaseUrl = o.value("custom_base_url").toString();
    if (o.contains("custom_model")) m_customModel = o.value("custom_model").toString();
    if (o.contains("custom_api_key")) m_customApiKey = decryptSecret(o.value("custom_api_key").toString());
    if (o.contains("clash_path")) m_clashPath = o.value("clash_path").toString();
    if (o.contains("v2ray_path")) m_v2rayPath = o.value("v2ray_path").toString();
    if (o.contains("user_name")) m_userName = o.value("user_name").toString();
    if (o.contains("avatar_char")) m_avatarChar = o.value("avatar_char").toString();
    if (o.contains("ai_name")) m_aiName = o.value("ai_name").toString();
    if (o.contains("ai_personality")) m_aiPersonality = o.value("ai_personality").toString();
    if (o.contains("allow_state_read")) m_allowStateRead = o.value("allow_state_read").toBool(true);
    if (o.contains("allow_time_record")) m_allowTimeRecord = o.value("allow_time_record").toBool(true);
    if (o.contains("allow_long_term_memory")) m_allowLongTermMemory = o.value("allow_long_term_memory").toBool(true);
    if (o.contains("wallpaper_blur_enabled")) m_wallpaperBlurEnabled = o.value("wallpaper_blur_enabled").toBool(true);
    if (o.contains("wallpaper_blur_radius")) m_wallpaperBlurRadius = qBound(0, o.value("wallpaper_blur_radius").toInt(24), 40);
    if (o.contains("wallpaper_brightness")) m_wallpaperBrightness = qBound(0.0, o.value("wallpaper_brightness").toDouble(0.5), 1.0);
    if (o.contains("appearance_mode")) m_appearanceMode = o.value("appearance_mode").toString();
    if (o.contains("wallpaper_glass_opacity")) m_wallpaperGlassOpacity = qBound(0.05, o.value("wallpaper_glass_opacity").toDouble(0.10), 0.20);
    f.close();
}

void ConfigService::save()
{
    QJsonObject o;
    o.insert("base_url", m_baseUrl);
    o.insert("model", m_model);
    o.insert("api_key", encryptSecret(m_apiKey));
    {
        QJsonObject encKeys;
        for (auto it = m_apiKeys.begin(); it != m_apiKeys.end(); ++it)
            encKeys.insert(it.key(), encryptSecret(it.value().toString()));
        o.insert("api_keys", encKeys);
    }
    o.insert("custom_base_url", m_customBaseUrl);
    o.insert("custom_model", m_customModel);
    o.insert("custom_api_key", encryptSecret(m_customApiKey));
    o.insert("clash_path", m_clashPath);
    o.insert("v2ray_path", m_v2rayPath);
    o.insert("user_name", m_userName);
    o.insert("avatar_char", m_avatarChar);
    o.insert("ai_name", m_aiName);
    o.insert("ai_personality", m_aiPersonality);
    o.insert("allow_state_read", m_allowStateRead);
    o.insert("allow_time_record", m_allowTimeRecord);
    o.insert("allow_long_term_memory", m_allowLongTermMemory);
    o.insert("wallpaper_blur_enabled", m_wallpaperBlurEnabled);
    o.insert("wallpaper_blur_radius", m_wallpaperBlurRadius);
    o.insert("wallpaper_brightness", m_wallpaperBrightness);
    o.insert("appearance_mode", m_appearanceMode);
    o.insert("wallpaper_glass_opacity", m_wallpaperGlassOpacity);
    QFile f(configPath());
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(QJsonDocument(o).toJson());
    f.close();
}

// ---- debounced persistence (setters are called from the main thread) ----
void ConfigService::scheduleSave()
{
    m_dirty = true;
    if (!m_saveTimer) {
        m_saveTimer = new QTimer();
        m_saveTimer->setSingleShot(true);
        m_saveTimer->setInterval(250);
        QObject::connect(m_saveTimer, &QTimer::timeout, [this]() {
            if (!m_dirty) return;
            m_dirty = false;
            save();
        });
    }
    if (!m_saveTimer->isActive()) m_saveTimer->start();
}

void ConfigService::flush()
{
    if (m_saveTimer && m_saveTimer->isActive()) m_saveTimer->stop();
    if (m_dirty) {
        m_dirty = false;
        save();
    }
}

QString ConfigService::detectClashExe() const
{
    const QStringList candidates = {
        "C:/Program Files/Clash Verge/clash-verge.exe",
        "C:/Program Files (x86)/Clash Verge/clash-verge.exe",
        "D:/Program Files/Clash Verge/clash-verge.exe",
        "D:/Clash Verge/clash-verge.exe",
        "E:/Clash Verge/clash-verge.exe",
        QDir::home().filePath("AppData/Local/Programs/clash-verge/clash-verge.exe"),
        QDir::home().filePath("AppData/Local/Clash Verge/clash-verge.exe"),
        QDir::home().filePath("Downloads/Clash Verge/clash-verge.exe"),
        QDir::home().filePath("Desktop/Clash Verge/clash-verge.exe"),
        "C:/Clash Verge/clash-verge.exe",
    };
    for (const QString &c : candidates)
        if (QFile::exists(c)) return c;

    // registry uninstall entries (HKCU + HKLM, both 64/32 views)
    const QStringList keys = {
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
    };
    for (const QString &k : keys) {
        QSettings s(k, QSettings::NativeFormat);
        for (const QString &sub : s.childGroups()) {
            s.beginGroup(sub);
            QString disp = s.value("DisplayName").toString();
            QString loc = s.value("InstallLocation").toString();
            QString exe = s.value("DisplayIcon").toString();
            s.endGroup();
            if (disp.contains("Clash", Qt::CaseInsensitive) || exe.contains("clash-verge", Qt::CaseInsensitive)) {
                // try install location first, then the DisplayIcon path
                QString p = loc + "/clash-verge.exe";
                if (QFile::exists(p)) return p;
                QString p2 = loc + "/Clash Verge.exe";
                if (QFile::exists(p2)) return p2;
                QString icon = exe.section(',', 0, 0);
                if (QFile::exists(icon)) return icon;
            }
        }
    }

    // portable Clash Verge (green build) fallback: search common roots
    const QStringList clashRoots = {
        QDir::homePath() + "/Desktop",
        QDir::homePath() + "/Desktop/梯子",
        QDir::homePath() + "/Downloads",
        "C:/",
        "D:/",
        "E:/",
    };
    for (const QString &root : clashRoots) {
        QDir dir(root);
        if (!dir.exists()) continue;
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &d : entries) {
            if (d.fileName().contains("clash", Qt::CaseInsensitive)) {
                QString p1 = d.absoluteFilePath() + "/clash-verge.exe";
                if (QFile::exists(p1)) return p1;
                QString p2 = d.absoluteFilePath() + "/Clash Verge.exe";
                if (QFile::exists(p2)) return p2;
                // portable Rev: clash-verge folder may contain a nested app dir
                QDir sub(d.absoluteFilePath());
                const auto subs = sub.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &sd : subs) {
                    QString deeper = sd.absoluteFilePath() + "/clash-verge.exe";
                    if (QFile::exists(deeper)) return deeper;
                }
            }
        }
    }
    return {};
}

QString ConfigService::detectV2rayExe() const
{
    const QString desk = QDir::homePath() + "/Desktop";
    const QString downloads = QDir::homePath() + "/Downloads";
    const QStringList candidates = {
        desk + "/梯子/v2rayN-windows-64-SelfContained/v2rayN.exe",
        desk + "/v2rayN-windows-64-SelfContained/v2rayN.exe",
        desk + "/v2rayN/v2rayN.exe",
        desk + "/梯子/v2rayN/v2rayN.exe",
        desk + "/梯子/v2rayN-windows-64/v2rayN.exe",
        downloads + "/v2rayN-windows-64-SelfContained/v2rayN.exe",
        downloads + "/v2rayN/v2rayN.exe",
        downloads + "/梯子/v2rayN-windows-64-SelfContained/v2rayN.exe",
        "C:/v2rayN/v2rayN.exe",
        "C:/Program Files/v2rayN/v2rayN.exe",
        "C:/Program Files (x86)/v2rayN/v2rayN.exe",
        "D:/v2rayN/v2rayN.exe",
        "D:/Program Files/v2rayN/v2rayN.exe",
        "E:/v2rayN/v2rayN.exe",
        QDir::home().filePath("AppData/Local/Programs/v2rayN/v2rayN.exe"),
    };
    for (const QString &c : candidates)
        if (QFile::exists(c)) return c;

    // registry uninstall entries
    const QStringList keys = {
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
        "HKEY_LOCAL_MACHINE\\Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
    };
    for (const QString &k : keys) {
        QSettings s(k, QSettings::NativeFormat);
        for (const QString &sub : s.childGroups()) {
            s.beginGroup(sub);
            QString disp = s.value("DisplayName").toString();
            QString loc = s.value("InstallLocation").toString();
            QString exe = s.value("DisplayIcon").toString();
            s.endGroup();
            if (disp.contains("v2ray", Qt::CaseInsensitive) || exe.contains("v2rayN", Qt::CaseInsensitive)) {
                QString p = loc + "/v2rayN.exe";
                if (QFile::exists(p)) return p;
                QString icon = exe.section(',', 0, 0);
                if (QFile::exists(icon)) return icon;
            }
        }
    }

    // v2rayN is commonly a green/portable build without registry entry:
    // search likely roots up to 3 levels deep, skipping huge dirs.
    const QStringList roots = {
        QDir::homePath() + "/Desktop",
        QDir::homePath() + "/Downloads",
        QDir::homePath() + "/Documents",
        QDir::homePath() + "/Desktop/梯子",
        "C:/",
        "D:/",
        "E:/",
    };
    for (const QString &root : roots) {
        QDir dir(root);
        if (!dir.exists()) continue;
        const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &d : entries) {
            if (d.fileName().contains("v2ray", Qt::CaseInsensitive)) {
                QString direct = d.absoluteFilePath() + "/v2rayN.exe";
                if (QFile::exists(direct)) return direct;
                // one level deeper (e.g. v2rayN-windows-64-SelfContained)
                QDir sub(d.absoluteFilePath());
                const auto subs = sub.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
                for (const QFileInfo &sd : subs) {
                    QString deeper = sd.absoluteFilePath() + "/v2rayN.exe";
                    if (QFile::exists(deeper)) return deeper;
                }
            }
        }
    }
    return {};
}
