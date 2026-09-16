// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QObject>
#include <QString>
#include <QHash>
#include <QJsonArray>
#include "ActivityMemory.h"
#include "MoodTrend.h"
#include "RelationshipState.h"
#include "MemoryStore.h"
class QTimer;
class ContextManager;
struct ConversationState;

// AI companion card: chat with DeepSeek + long-term memory.
// Memory: JSON file storing session times, novel-edit counts, sleep habits.
// On morning first launch, generates a greeting like:
//   "早上好，用户。昨天你的OpenCode改代码8小时。你的小说文档昨天修改了三次。另外……你又凌晨三点睡觉。"
class AiService : public QObject
{
    Q_OBJECT
public:
    explicit AiService(QObject *parent = nullptr);

    Q_INVOKABLE QString greeting();          // morning greeting from memory
    Q_INVOKABLE bool shouldGreetToday();     // only greet on the first launch of the day
    Q_INVOKABLE void markGreeted();          // record today as greeted
    Q_INVOKABLE void sendMessage(const QString &text); // async chat -> chatReply
    Q_INVOKABLE bool apiConfigured();        // api key + base url are filled in
    Q_INVOKABLE bool apiOnline();            // configured and the last request succeeded
    Q_INVOKABLE void setChatHistory(const QString &history); // seed recent-chat context (from UI/SQLite)
    Q_INVOKABLE QString memoryReport();      // what the AI remembers (for settings/tests)
    Q_INVOKABLE QString uptimeText();        // how long the PC has been on
    Q_INVOKABLE void generateGreeting();     // async: DeepSeek writes greeting -> greetingReady
    Q_INVOKABLE bool isGameRunning();        // detects r5apex / minecraft etc.
    Q_INVOKABLE bool isFullscreenGame();     // foreground window covers the whole screen (any game)
    Q_INVOKABLE bool isForegroundMinecraft(); // foreground window is Minecraft
    Q_INVOKABLE void idleChat();             // async: AI initiates a topic -> chatReply
    Q_INVOKABLE qint64 lastInputMs();        // ms since the last keyboard/mouse input system-wide

    // ---- batch A: proactive conversation engine ----
    Q_INVOKABLE bool isUserBusy();           // true if coding/typing-heavy foreground
    Q_INVOKABLE QString userActivityState(); // "coding"/"gaming"/"idle"/"away"/"late_night"
    Q_INVOKABLE void trackUnfinishedTopic(const QString &topic, int importance); // save unfinished topic
    Q_INVOKABLE QString unfinishedTopicsText(int max); // recent unfinished topics for prompt
    Q_INVOKABLE void recordInterest(const QString &interest); // bump interest weight
    Q_INVOKABLE QString interestsText(int max); // top interests for prompt
    Q_INVOKABLE QString aiStateJson();       // {"mood":"calm","energy":80}
    Q_INVOKABLE void setAiMood(const QString &mood); // update AI mood (continuity)
    Q_INVOKABLE void adjustAiEnergy(int delta);      // energy after interactions

    // ---- batch B: relationship ----
    Q_INVOKABLE QString relationshipText();  // relationship continuity for prompt
    Q_INVOKABLE void bumpRelationship(int intimacyDelta, int trustDelta);
    Q_INVOKABLE void setRelationship(int intimacy, int trust); // manual override
    Q_INVOKABLE int relationshipIntimacy();
    Q_INVOKABLE int relationshipTrust();
    Q_INVOKABLE QString interestsRaw();      // interests.json as text
    Q_INVOKABLE void setInterestsRaw(const QString &json);
    Q_INVOKABLE QStringList interestList();  // friendly list
    Q_INVOKABLE void addInterest(const QString &name);
    Q_INVOKABLE void removeInterest(const QString &name);
    Q_INVOKABLE QString unfinishedRaw();     // unfinished_topics.json as text
    Q_INVOKABLE void setUnfinishedRaw(const QString &json);
    Q_INVOKABLE QStringList topicList();     // friendly list
    Q_INVOKABLE void addTopic(const QString &topic);
    Q_INVOKABLE void removeTopic(const QString &topic);
    Q_INVOKABLE void setAiEnergy(int energy); // manual energy override
    Q_INVOKABLE QString notesText();        // user notes only
    Q_INVOKABLE QStringList noteList();     // friendly list
    Q_INVOKABLE void removeNote(int index);
    Q_INVOKABLE QString usageText();        // usage duration only

    // user profile passthrough (stored in config)
    Q_INVOKABLE QString userName();
    Q_INVOKABLE QString avatarChar();
    Q_INVOKABLE void setUserName(const QString &v);
    Q_INVOKABLE void setAvatarChar(const QString &v);

    // current contact (AI) profile — delegated to ContactService
    Q_INVOKABLE QString aiName();
    Q_INVOKABLE QString aiPersonality();
    Q_INVOKABLE void setAiName(const QString &v);
    Q_INVOKABLE void setAiPersonality(const QString &v);
    Q_INVOKABLE QString setAiAvatar(const QString &srcPath);
    Q_INVOKABLE QString aiAvatarPath();

    // AI API config passthrough
    Q_INVOKABLE QString apiBaseUrl();
    Q_INVOKABLE QString apiModel();
    Q_INVOKABLE QString apiKey();
    Q_INVOKABLE void setApiBaseUrl(const QString &v);
    Q_INVOKABLE void setApiModel(const QString &v);
    Q_INVOKABLE void setApiKey(const QString &v);
    // per-provider key memory (preset switching restores the right key)
    Q_INVOKABLE QString apiKeyFor(const QString &baseUrl);
    Q_INVOKABLE void rememberApiKeyFor(const QString &baseUrl, const QString &key);
    // query {base}/models with the given key -> supported model ids ("" if error)
    Q_INVOKABLE QStringList fetchAvailableModels(const QString &baseUrl, const QString &apiKey);
    // dedicated custom slot (never overwritten by presets)
    Q_INVOKABLE QString customBaseUrl();
    Q_INVOKABLE QString customModel();
    Q_INVOKABLE QString customApiKey();
    Q_INVOKABLE void setCustomApi(const QString &url, const QString &model, const QString &key);

    // avatar images: copy a local image into app data, return stored path
    Q_INVOKABLE QString setUserAvatar(const QString &srcPath);
    Q_INVOKABLE QString userAvatarPath();

    // ---- custom wallpaper (blurred copy shown behind the right content pane) ----
    Q_INVOKABLE QString wallpaperPath();                  // file:// url of the effective copy ("" if none)
    Q_INVOKABLE QString setWallpaper(const QString &srcPath); // copy + blur -> returns wallpaperPath()
    Q_INVOKABLE QString setWallpaperPreset(int index);    // built-in dark gradient (0..3)
    Q_INVOKABLE void removeWallpaper();
    Q_INVOKABLE bool wallpaperBlurEnabled();
    Q_INVOKABLE void setWallpaperBlurEnabled(bool v);     // regenerates the blurred copy
    Q_INVOKABLE int wallpaperBlurRadius();
    Q_INVOKABLE void setWallpaperBlurRadius(int r);       // 0..40, regenerates the blurred copy
    Q_INVOKABLE double wallpaperBrightness();             // 0..1 average luminance (drives dark overlay)
    Q_INVOKABLE QString wallpaperTintColor();             // "#rrggbb" average colour — glass-mode environment tint

    // ---- appearance mode (wallpaper glass) ----
    Q_INVOKABLE QString appearanceMode();                 // "" (默认深色) | "glass" (壁纸玻璃)
    Q_INVOKABLE void setAppearanceMode(const QString &v);
    Q_INVOKABLE double wallpaperGlassOpacity();           // wallpaper layer opacity 0.05..0.20
    Q_INVOKABLE void setWallpaperGlassOpacity(double v);

    // lifecycle: call on app start / exit to record sessions
    Q_INVOKABLE void recordSessionStart();
    Q_INVOKABLE void recordSessionEnd();

    // ---- AIRI-style memory management ----
    Q_INVOKABLE QString memoryDetail();                 // full human-readable memory (settings view)
    Q_INVOKABLE void addMemoryNote(const QString &note); // user tells AI something to remember
    Q_INVOKABLE void clearMemory();                      // wipe memory (keep created stamp)
    Q_INVOKABLE void setMemoryRaw(const QString &json);  // replace memory file (keep created) — for manual editing
    Q_INVOKABLE QString memoryRaw();                     // raw memory.json as text (for editing)

    // ---- companion: relationship state + event memory (batch 1) ----
    Q_INVOKABLE void recordEvent(const QString &type, const QString &summary); // event memory (with date)
    Q_INVOKABLE QString eventMemoryText(int maxEvents); // recent events for prompt injection

    Q_INVOKABLE QString foregroundApp();                 // current foreground window title (lightweight)

    // ---- batch 2: system state sensing + analysis ----
    Q_INVOKABLE QString systemStateJson();               // {cpu, mem, hour, foreground, idleMs, ...}
    Q_INVOKABLE QString analyzeUserState();              // human state: gaming/working/late_night/away/relaxed
    Q_INVOKABLE QString batteryState();                  // battery percent + plugged (or empty on desktop)

    // ---- batch 3: emotion + response strategy ----
    Q_INVOKABLE QString inferUserEmotion(const QString &text); // happy/tired/stressed/lonely/normal

    // privacy toggles (forwarded to ConfigService for QML access)
    Q_INVOKABLE bool allowStateRead();
    Q_INVOKABLE bool allowTimeRecord();
    Q_INVOKABLE bool allowLongTermMemory();
    Q_INVOKABLE void setAllowStateRead(bool v);
    Q_INVOKABLE void setAllowTimeRecord(bool v);
    Q_INVOKABLE void setAllowLongTermMemory(bool v);

    // ---- PC activity monitor (startup/shutdown + software usage) ----
    Q_INVOKABLE void startActivityMonitor();   // begin periodic foreground-app sampling
    Q_INVOKABLE void stopActivityMonitor();
    Q_INVOKABLE QString activitySummary();     // "今天大部分时间在用什么" (from short-term memory)

    // ---- batch 4: desktop-sensing triggers + usage depth (v3.9.1) ----
    Q_INVOKABLE QString longestRunningAppToday();      // top-used app today (excl. proxy/explorer/system)
    Q_INVOKABLE QString currentForegroundSessionText();// "你正在使用 X，已经用了约 Y 分钟"
    Q_INVOKABLE QString yesterdayActivitySummary();    // yesterday's archived top apps (with minutes)
    Q_INVOKABLE QString dailyReportData();             // home "AI 日报" raw values (JSON: usage/topApp/shutdown/advice)
    Q_INVOKABLE void generateDailyAdvice();            // async: AI writes today's one-line tip -> adviceReady

    // ---- batch 5: silence / proactive gates (v3.9.2) ----
    Q_INVOKABLE bool isDoNotDisturb();                 // proactive chat banned right now
    Q_INVOKABLE QString doNotDisturbReason();          // "gaming"/"fullscreen"/"meeting"/"work"
    Q_INVOKABLE int lastProactiveScore();              // last computed proactive score (debug/UI)

    // ---- companion context management (facts / corrections) ----
    Q_INVOKABLE void notifyUserCorrection();   // UI hint: last AI claim was wrong (debug)
    Q_INVOKABLE int  correctionCount();        // how many corrections this session

    // ---- proactive topic ranking (for tests / future UI) ----
    Q_INVOKABLE QStringList rankedTopics(int max); // top N proactive topics by score

signals:
    void chatReply(QString text);
    void adviceReady(QString text);          // home daily-report tip (AI-generated)
    void apiStatusChanged();                 // API reachability changed -> presence dot
    void idleReply(QString text);            // AI initiated a chat on its own (proactive)
    void thinkingReady(QString text);
    void greetingReady(QString text);
    void emotionSignal(QString emotion, qreal intensity); // AIRI-style ACT token playback
    void profileChanged();                               // name/avatar/persona changed -> refresh UI
    void wallpaperChanged();                             // custom wallpaper set/removed -> refresh backdrop
    void doNotDisturbChanged();                          // DND state changed (UI may show a badge)

private:
    QString memoryPath() const;
    void ensureMemory();
    QString readMemory() const;
    void writeMemory(const QString &json);
    static QString callDeepSeekStatic(const QString &system, const QString &user);
    static QString callDeepSeekMessages(const QJsonArray &messages);
    QString jsonGet(const QString &json, const QString &key);
    QString jsonSet(const QString &json, const QString &key, const QString &value);

    bool m_apiOnline = true;        // optimistic; flips false on a failed request
    void setApiOnline(bool v);

    // auto memory: after N user turns, summarize the recent chat into a short note
    void trackChatTurn(const QString &userText, const QString &aiReply);
    void maybeSummarize();
    void appendNote(const QString &note);
    void appendNote(const QString &note, const CoreMemory &core); // v5.0: with personal-model fields
    void dedupMemoryNotes();   // drop duplicate note cores on startup

    // ---- v5.0 Memory Core: annotate a memory with WHY it matters ----
    // Any field left empty / negative is filled by heuristics, so callers
    // may pass only what the structured LLM produced.
    void saveCoreMemory(const QString &text, const QString &source,
                        const QString &category = QString(),
                        const QString &emotion = QString(),
                        const QString &relationship = QString(),
                        double importance = -1.0, double confidence = -1.0);
    QString personalModelBlock(int maxItems = 6, const QString &currentEmotion = QString()) const; // prompt-ready "why it matters" lines
    void maybeRecordSharedMoment(const QString &userText, const QString &emotion); // 共同经历 milestones (first / reunion / heart-to-heart)
    static QString postChatCompletion(const QJsonArray &messages);   // shared synchronous LLM POST

    // ---- v5.2: read-only agent tools (network / system / processes / self) ----
    // Detects a [[tool:id]] directive in the model's raw reply, runs the tool
    // off-thread, then asks the model for the final in-character answer.
    // Returns true when it took over the turn (an async path is in flight).
    bool maybeHandleAgentTool(const QString &raw, const QJsonArray &baseMsgs,
                              const QString &userText, const QString &emotion,
                              const QString &factsText);

    QStringList m_chatBuffer;   // recent turns (user/ai pairs), bounded
    int m_userTurns = 0;        // user messages since last summary
    bool m_summarizing = false;

    // v5.2: rebind every per-contact path/cache when the active AI changes.
    // Without this the sidecar files (ledger / conversation state / relationship)
    // and the chat buffer keep pointing at the PREVIOUS AI.
    void syncActiveContact();
    QString m_stateContactId;   // contact the per-contact state is bound to

    // activity monitor internals
    void recordActivitySample();      // called by the timer
    void flushActivityToMemory();     // write the accumulated usage into memory
    QHash<QString, int> m_appMinutes; // app title -> minutes (today)
    QTimer *m_monitorTimer = nullptr;

    // ---- v3.9.1: desktop-sensing internals ----
    QString desktopStateBlock(bool force);            // prompt-ready real-time desktop info
    bool matchesDesktopTriggers(const QString &text); // "猜在干嘛 / 用多久 / 关心类"
    void applyMemoryDecay();                          // old notes -> short gist (只记得大概)
    QString m_fgApp;              // currently-tracked foreground app
    qint64 m_fgSinceMs = 0;       // epoch ms when m_fgApp became foreground
    QString m_lastEmotion;        // previous turn's emotion (mood-change detection)

    // v3.9.2: short-term usage memory (separate file, not personality memory)
    ActivityMemory m_activityMemory;

    // v3.9.2: silence / proactive gates
    QString m_dndReason;              // "" when not do-not-disturb
    int m_lastProactiveScore = 0;     // last computed proactive score
    bool m_userJustRefused = false;   // user recently declined a chat
    qint64 m_refusedAtMs = 0;         // when they refused (cooldown)
    QString m_lastFgCategory;         // previous foreground category (task-finish detection)
    bool m_justFinishedTask = false;  // just switched from work/gaming to relaxing
    qint64 m_justFinishedTaskAtMs = 0;// when that happened

    // v4.3: proactive message quality gates (读空气 + 冷却防复读 + 间隔)
    int     m_unansweredProactive = 0;    // consecutive proactive msgs user didn't reply to
    qint64  m_lastProactiveAtMs = 0;      // when the last proactive message was sent
    qint64  m_lastUserReplyAtMs = 0;      // when the user last replied (read-the-air)

    // v3.9.2: mood trend + relationship state (phase 3)
    MoodTrend m_moodTrend;            // last-10-chat emotion statistics
    RelationshipState m_relationshipState; // relationship_state.json

    // companion context: short-term fact cache + correction ledger
    ContextManager *m_ctx = nullptr;
    QString m_lastAiReply;            // last AI reply (for correction detection)

    // short-term conversation state (current session only, never persisted)
    ConversationState *m_convo = nullptr;
    void updateConversationState(const QString &userText, const QString &aiReply, const QString &emotion);
    // v4.3: shared post-reply pipeline (emotion tokens, PAD integration,
    // chatReply emit, memory tracking) — used by single- and double-pass paths
    void deliverReply(const QString &speech, const QString &userText, const QString &emotion);
    QString conversationStateBlock() const;  // prompt-ready state section
    QString worldInfoBlock(const QString &text); // keyword-triggered lorebook (话题/事件)
    QString timeAwarenessBlock() const;      // current date/weekday/daypart + idle gap since last turn
    QString openLoopPromptBlock() const;     // due open loops (promises to follow up), prompt-ready

    // v3.9: conflict resolution + importance-gated memory write
    void runConflictResolution(const QString &userText, const QString &memJson);
    void bumpRecalledUsage(const QString &userMsg, const QString &topic);

    void regenerateWallpaper();      // re-blur wallpaper.png at the current radius
    void applyWallpaperBlur();       // debounced regenerate + notify (main thread)
    QTimer *m_wallpaperDebounce = nullptr;
};
