// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include <QFont>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QtQuickControls2/QQuickStyle>
#include <QQuickWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

#include "Core/AppCore.h"
#include "Services/ProxyService.h"
#include "Services/NetworkService.h"
#include "Services/SystemService.h"
#include "Services/MoodService.h"
#include "Services/ConfigService.h"
#include "Services/UpdateService.h"
#include "Services/SyncService.h"
#include "Services/PluginManager.h"
#include "Services/AiService.h"
#include "Services/StatsService.h"
#include "Services/ContactService.h"
#include "Services/AgentToolRegistry.h"
#include <QTextStream>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("XiaoQin");
    // NOTE: intentionally kept as "XiaoQinTools" after the Solace rename so the
    // %APPDATA%\XiaoQin\XiaoQinTools data directory (config + AI memory) is preserved.
    app.setApplicationName("XiaoQinTools");
    app.setApplicationVersion(NIGHTLOG_VERSION);
    app.setWindowIcon(QIcon(":/icons/app.ico"));

    // ---- dev self-check: run every read-only agent tool once and print it ----
    // usage: Solace.exe --selftest-tools
    if (app.arguments().contains(QStringLiteral("--selftest-tools"))) {
        AgentToolRegistry &tools = AgentToolRegistry::instance();
        QString report = tools.catalogText() + "\n\n";
        const QStringList ids{QStringLiteral("net.diagnose"), QStringLiteral("sys.info"),
                              QStringLiteral("sys.processes"), QStringLiteral("app.selfcheck")};
        for (const QString &id : ids) {
            const ToolResult result = tools.run(id, QJsonObject());
            report += "--- " + id + " (ok=" + (result.ok ? "true" : "false") + ") ---\n"
                    + result.text + "\n\n";
        }
        // the app is a WIN32-subsystem binary, so also drop the report on disk
        QFile reportFile(QDir::tempPath() + QStringLiteral("/solace-selftest.txt"));
        if (reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            reportFile.write(report.toUtf8());
        QTextStream(stdout) << report;
        return 0;
    }

    // ---- single instance guard ----
    // A second instance would hold a stale ConfigService snapshot and its
    // next save() would clobber whatever the first instance wrote (the
    // "settings don't stick" bug). Named mutex works across integrity
    // levels (elevated vs normal), unlike a QLocalServer pipe.
#ifdef Q_OS_WIN
    HANDLE instanceMutex = CreateMutexW(nullptr, TRUE, L"Local\\XiaoQinToolsSingleInstance");
    if (instanceMutex && GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;
#endif

    QFont f("Microsoft YaHei UI");
    f.setPixelSize(14);
    app.setFont(f);

    // Fusion style honors the dark palette set from QML -> dark TextFields/Groups.
    QQuickStyle::setStyle("Fusion");

    // NOTE: services are injected as root context properties below — QML never
    // instantiates them as components, so no qmlRegisterType calls are needed.

    QQmlApplicationEngine engine;
    // Make QML modules resolvable next to the executable (deployed Qt plugins).
    QString importPath = QCoreApplication::applicationDirPath() + "/qml";
    engine.addImportPath(importPath);
    engine.addImportPath("qrc:/");
    engine.rootContext()->setContextProperty("appCore", &AppCore::instance());
    engine.rootContext()->setContextProperty("proxyService", new ProxyService(&engine));
    engine.rootContext()->setContextProperty("netService", new NetworkService(&engine));
    engine.rootContext()->setContextProperty("sysService", new SystemService(&engine));
    engine.rootContext()->setContextProperty("moodService", new MoodService(&engine));
    engine.rootContext()->setContextProperty("updateService", new UpdateService(&engine));
    engine.rootContext()->setContextProperty("syncService", new SyncService(&engine));
    engine.rootContext()->setContextProperty("pluginManager", new PluginManager(&engine));
    auto *aiSvc = new AiService(&engine);
    engine.rootContext()->setContextProperty("aiService", aiSvc);
    aiSvc->recordSessionStart();
    aiSvc->startActivityMonitor();
    engine.rootContext()->setContextProperty("statsService", new StatsService(&engine));
    engine.rootContext()->setContextProperty("contactService", &ContactService::instance());

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    engine.load(QUrl("qrc:/qml/Main.qml"));

#ifdef Q_OS_WIN
    // frameless: ask DWM to round the window corners natively (Windows 11).
    // The window itself stays OPAQUE, so DWM's corner clip produces real
    // rounded corners with the desktop showing through — no QML shaders needed.
    if (!engine.rootObjects().isEmpty()) {
        if (auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            HWND hwnd = reinterpret_cast<HWND>(win->winId());
            const DWORD round = 2; // DWMWCP_ROUND
            DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/,
                                  &round, sizeof(round));
            // Frameless windows lose the DWM minimize/restore taskbar animation
            // because they lack WS_CAPTION|WS_MINIMIZEBOX. Add the style bits
            // back: DWM plays the system animation again while Qt keeps drawing
            // no title bar (the standard Electron-style hidden-titlebar hack).
            const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
            SetWindowLongPtrW(hwnd, GWL_STYLE, style | WS_CAPTION | WS_MINIMIZEBOX);
        }
    }
#endif

    QObject::connect(&app, &QCoreApplication::aboutToQuit, aiSvc, [aiSvc]() {
        aiSvc->recordSessionEnd();
        aiSvc->stopActivityMonitor();
        // flush any debounced config write so the last edit is not lost
        ConfigService::instance().flush();
    });
    return app.exec();
}
