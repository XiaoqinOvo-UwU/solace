#pragma once
#include <QString>
#include <QJsonObject>
#include <QtGlobal>

class QTimer;

// Load/save app config to a JSON file in the user config dir.
// Ported concept from the WinForms AppConfig (base_url / model / api_key / proxy paths).
class ConfigService
{
public:
    static ConfigService &instance();

    // ---- secret storage (Windows DPAPI) ----
    // Secrets are stored on disk encrypted with the current Windows user's
    // DPAPI key, prefixed "dpapi:v1:". Non-Windows falls back to passthrough.
    // decryptSecret() transparently accepts legacy plaintext values.
    static QString encryptSecret(const QString &plain);
    static QString decryptSecret(const QString &stored);

    QString baseUrl() const { return m_baseUrl; }
    QString model() const { return m_model; }
    QString apiKey() const { return m_apiKey; }

    // per-provider key memory: baseUrl -> last used api key (preset switching)
    QString apiKeyFor(const QString &baseUrl) const { return m_apiKeys.value(baseUrl).toString(); }
    void rememberApiKeyFor(const QString &baseUrl, const QString &key) { m_apiKeys.insert(baseUrl, key); scheduleSave(); }

    // dedicated CUSTOM slot — the user's own endpoint, kept separate from
    // presets so switching DeepSeek/OpenAI and back never loses it
    QString customBaseUrl() const { return m_customBaseUrl; }
    QString customModel() const { return m_customModel; }
    QString customApiKey() const { return m_customApiKey; }
    void setCustomBaseUrl(const QString &v) { m_customBaseUrl = v; scheduleSave(); }
    void setCustomModel(const QString &v) { m_customModel = v; scheduleSave(); }
    void setCustomApiKey(const QString &v) { m_customApiKey = v; scheduleSave(); }

    QString clashPath() const { return m_clashPath; }
    QString v2rayPath() const { return m_v2rayPath; }

    // user profile
    QString userName() const { return m_userName; }
    QString avatarChar() const { return m_avatarChar; }
    QString aiName() const { return m_aiName; }
    QString aiPersonality() const { return m_aiPersonality; }

    // privacy toggles (default ON)
    bool allowStateRead() const { return m_allowStateRead; }
    bool allowTimeRecord() const { return m_allowTimeRecord; }
    bool allowLongTermMemory() const { return m_allowLongTermMemory; }
    void setAllowStateRead(bool v) { m_allowStateRead = v; scheduleSave(); }
    void setAllowTimeRecord(bool v) { m_allowTimeRecord = v; scheduleSave(); }
    void setAllowLongTermMemory(bool v) { m_allowLongTermMemory = v; scheduleSave(); }

    // wallpaper blur (default on, radius 24)
    bool wallpaperBlurEnabled() const { return m_wallpaperBlurEnabled; }
    int  wallpaperBlurRadius() const { return m_wallpaperBlurRadius; }
    void setWallpaperBlurEnabled(bool v) { m_wallpaperBlurEnabled = v; scheduleSave(); }
    void setWallpaperBlurRadius(int v) { m_wallpaperBlurRadius = qBound(0, v, 40); scheduleSave(); }

    // wallpaper average luminance (0=dark .. 1=bright) — drives the dark overlay
    double wallpaperBrightness() const { return m_wallpaperBrightness; }
    void setWallpaperBrightness(double v) { m_wallpaperBrightness = qBound(0.0, v, 1.0); scheduleSave(); }

    // appearance mode: "" = 默认深色 | "glass" = 壁纸玻璃
    QString appearanceMode() const { return m_appearanceMode; }
    double  wallpaperGlassOpacity() const { return m_wallpaperGlassOpacity; }
    void setAppearanceMode(const QString &v) { m_appearanceMode = v; scheduleSave(); }
    void setWallpaperGlassOpacity(double v) { m_wallpaperGlassOpacity = qBound(0.05, v, 0.20); scheduleSave(); }

    void setBaseUrl(const QString &v) { m_baseUrl = v; scheduleSave(); }
    void setModel(const QString &v) { m_model = v; scheduleSave(); }
    void setApiKey(const QString &v) { m_apiKey = v; scheduleSave(); }
    void setUserName(const QString &v) { m_userName = v; scheduleSave(); }
    void setAvatarChar(const QString &v) { m_avatarChar = v; scheduleSave(); }
    void setAiName(const QString &v) { m_aiName = v; scheduleSave(); }
    void setAiPersonality(const QString &v) { m_aiPersonality = v; scheduleSave(); }

    QString configDir() const;
    QString configPath() const;

    void load();
    // Debounced save: setters mark dirty and a 250ms single-shot timer writes
    // the file, so slider drags etc. don't hammer the disk. flush() forces an
    // immediate write (called on app quit).
    void scheduleSave();
    void flush();
    void save();

    // auto-detect proxy install paths (ported from FindClashExe/FindV2rayExe)
    QString detectClashExe() const;
    QString detectV2rayExe() const;

private:
    ConfigService() { load(); }
    QString m_baseUrl = "https://api.deepseek.com/v1";
    QString m_model = "deepseek-chat";
    QString m_apiKey;
    QJsonObject m_apiKeys;             // baseUrl -> last used key
    QString m_customBaseUrl;           // user's own endpoint (自定义 slot)
    QString m_customModel;
    QString m_customApiKey;
    QString m_clashPath;
    QString m_v2rayPath;
    QString m_userName = "用户";
    QString m_avatarChar = "用";
    QString m_aiName = "AI助手";
    QString m_aiPersonality = "温柔、可爱、像朋友";
    bool m_allowStateRead = true;
    bool m_allowTimeRecord = true;
    bool m_allowLongTermMemory = true;
    bool m_wallpaperBlurEnabled = true;
    int  m_wallpaperBlurRadius = 12;   // subtle frosted by default
    double m_wallpaperBrightness = 0.5;
    QString m_appearanceMode;          // "" | "glass"
    double m_wallpaperGlassOpacity = 0.10; // wallpaper layer opacity in glass mode
    QTimer *m_saveTimer = nullptr;     // debounced persist timer (250ms single-shot)
    bool m_dirty = false;              // config changed since last write
};
