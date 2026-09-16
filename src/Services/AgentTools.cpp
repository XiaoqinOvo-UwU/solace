// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "AgentTools.h"
#include "ConfigService.h"
#include <QCoreApplication>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QHostInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QThread>
#include <algorithm>
#include <windows.h>

// ============================ shared helpers ============================

static QString tcpProbe(const QString &host, quint16 port)
{
    QTcpSocket socket;
    QElapsedTimer timer;
    timer.start();
    socket.connectToHost(host, port);
    const bool connected = socket.waitForConnected(2500);
    const qint64 ms = timer.elapsed();
    socket.abort();
    if (connected)
        return QString("- 连接 %1:%2：成功 %3ms").arg(host).arg(port).arg(ms);
    return QString("- 连接 %1:%2：失败/超时（%3ms）").arg(host).arg(port).arg(ms);
}

static int cpuLoadPercent()
{
    auto readTimes = [](quint64 *idle, quint64 *total) -> bool {
        FILETIME idleTime, kernelTime, userTime;
        if (!GetSystemTimes(&idleTime, &kernelTime, &userTime))
            return false;
        auto toU64 = [](const FILETIME &ft) {
            return (static_cast<quint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
        };
        *idle = toU64(idleTime);
        *total = toU64(kernelTime) + toU64(userTime);
        return true;
    };

    quint64 idle1 = 0, total1 = 0;
    if (!readTimes(&idle1, &total1))
        return -1;
    QThread::msleep(200);
    quint64 idle2 = 0, total2 = 0;
    if (!readTimes(&idle2, &total2))
        return -1;

    const quint64 idleDelta = idle2 - idle1;
    const quint64 totalDelta = total2 - total1;
    if (totalDelta == 0)
        return -1;
    return static_cast<int>(100 - (idleDelta * 100 / totalDelta));
}

// ============================ net.diagnose ============================

QString NetworkDiagnoseTool::id() const { return QStringLiteral("net.diagnose"); }

QString NetworkDiagnoseTool::description() const
{
    return QStringLiteral(
        "检查本机网络：DNS 解析、常用服务连接延迟、系统代理是否开启"
        "（用户说网络卡/慢/连不上/延迟高时用）");
}

ToolResult NetworkDiagnoseTool::run(const QJsonObject &) const
{
    QStringList lines;

    const QString dnsHost = QStringLiteral("api.deepseek.com");
    QElapsedTimer timer;
    timer.start();
    const QHostInfo info = QHostInfo::fromName(dnsHost);
    const qint64 dnsMs = timer.elapsed();
    if (info.error() == QHostInfo::NoError && !info.addresses().isEmpty())
        lines << QString("- DNS %1：%2ms（%3）").arg(dnsHost).arg(dnsMs)
                     .arg(info.addresses().first().toString());
    else
        lines << QString("- DNS %1：失败（%2）").arg(dnsHost, info.errorString());

    lines << tcpProbe(QStringLiteral("api.deepseek.com"), 443);
    lines << tcpProbe(QStringLiteral("api.github.com"), 443);
    lines << tcpProbe(QStringLiteral("www.baidu.com"), 443);

    QSettings sysProxy(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings"),
        QSettings::NativeFormat);
    const bool proxyOn = sysProxy.value(QStringLiteral("ProxyEnable")).toInt() != 0;
    const QString proxy = sysProxy.value(QStringLiteral("ProxyServer")).toString();
    lines << QString("- 系统代理：%1").arg(proxyOn ? ("已开启 " + proxy)
                                                   : QStringLiteral("未开启"));

    return {true, QStringLiteral("网络诊断（本机实测）：\n") + lines.join('\n')};
}

// ============================ sys.info ============================

QString SystemInfoTool::id() const { return QStringLiteral("sys.info"); }

QString SystemInfoTool::description() const
{
    return QStringLiteral(
        "查看本机资源：内存占用、C 盘剩余、CPU 占用、开机时长"
        "（用户问电脑卡不卡/内存够不够时用）");
}

ToolResult SystemInfoTool::run(const QJsonObject &) const
{
    QStringList lines;

    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem))
        lines << QString("- 内存：已用 %1%（总 %2 GB，可用 %3 GB）")
                     .arg(mem.dwMemoryLoad)
                     .arg(mem.ullTotalPhys / 1073741824.0, 0, 'f', 1)
                     .arg(mem.ullAvailPhys / 1073741824.0, 0, 'f', 1);

    ULARGE_INTEGER freeBytes, totalBytes, totalFree;
    if (GetDiskFreeSpaceExW(L"C:\\", &freeBytes, &totalBytes, &totalFree))
        lines << QString("- C 盘：可用 %1 GB / 共 %2 GB")
                     .arg(totalFree.QuadPart / 1073741824.0, 0, 'f', 1)
                     .arg(totalBytes.QuadPart / 1073741824.0, 0, 'f', 1);

    const qint64 upSeconds = static_cast<qint64>(GetTickCount64() / 1000);
    lines << QString("- 开机时长：%1 小时 %2 分钟")
                 .arg(upSeconds / 3600).arg((upSeconds % 3600) / 60);

    const int cpu = cpuLoadPercent();
    if (cpu >= 0)
        lines << QString("- CPU 占用：约 %1%").arg(cpu);

    return {true, QStringLiteral("系统状态（本机实测）：\n") + lines.join('\n')};
}

// ============================ sys.processes ============================

QString ProcessListTool::id() const { return QStringLiteral("sys.processes"); }

QString ProcessListTool::description() const
{
    return QStringLiteral(
        "列出当前占用内存最高的进程"
        "（用户问后台谁在吃资源/是不是有脏东西时用）");
}

ToolResult ProcessListTool::run(const QJsonObject &) const
{
    QProcess tasklist;
    tasklist.start(QStringLiteral("tasklist"),
                   QStringList() << QStringLiteral("/fo") << QStringLiteral("csv")
                                 << QStringLiteral("/nh"));
    if (!tasklist.waitForFinished(6000))
        return {false, QStringLiteral("读取进程列表超时")};

    const QString out = QString::fromLocal8Bit(tasklist.readAllStandardOutput());
    static const QRegularExpression row(QStringLiteral(
        "\"([^\"]+)\",\"(\\d+)\",\"[^\"]*\",\"[^\"]*\",\"([\\d,]+)\\s*K\""));

    QList<QPair<qint64, QString>> procs;   // (KB, "name (pid N)")
    int total = 0;
    auto match = row.globalMatch(out);
    while (match.hasNext()) {
        const auto m = match.next();
        const qint64 kb = m.captured(3).remove(',').toLongLong();
        procs.append({kb, QStringLiteral("%1 (pid %2)").arg(m.captured(1), m.captured(2))});
        ++total;
    }
    if (procs.isEmpty())
        return {false, QStringLiteral("没有解析到进程信息")};

    std::sort(procs.begin(), procs.end(),
              [](const QPair<qint64, QString> &a, const QPair<qint64, QString> &b) {
                  return a.first > b.first;
              });

    QStringList lines;
    const int top = qMin(8, static_cast<int>(procs.size()));
    for (int i = 0; i < top; ++i)
        lines << QString("- %1：%2 MB")
                     .arg(procs.at(i).second)
                     .arg(procs.at(i).first / 1024.0, 0, 'f', 0);
    lines << QStringLiteral("（当前共 %1 个进程）").arg(total);

    return {true, QStringLiteral("占用内存最高的进程：\n") + lines.join('\n')};
}

// ============================ app.selfcheck ============================

QString SelfCheckTool::id() const { return QStringLiteral("app.selfcheck"); }

QString SelfCheckTool::description() const
{
    return QStringLiteral(
        "检查 Solace 自身：版本、数据目录体积、API 配置与记忆文件状态"
        "（用户问你有没有问题/让你自检时用）");
}

ToolResult SelfCheckTool::run(const QJsonObject &) const
{
    QStringList lines;
    lines << QString("- 版本：%1").arg(QCoreApplication::applicationVersion());

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    qint64 bytes = 0;
    int files = 0;
    QDirIterator walk(dir, QDir::Files, QDirIterator::Subdirectories);
    while (walk.hasNext()) {
        walk.next();
        bytes += walk.fileInfo().size();
        ++files;
    }
    lines << QString("- 数据目录：%1（%2 个文件，%3 MB）")
                 .arg(dir).arg(files).arg(bytes / 1048576.0, 0, 'f', 1);

    const bool apiReady = !ConfigService::instance().apiKey().trimmed().isEmpty()
                       && !ConfigService::instance().baseUrl().trimmed().isEmpty();
    lines << QString("- API 配置：%1").arg(apiReady ? QStringLiteral("完整")
                                                    : QStringLiteral("不完整（缺 Key 或 Base URL）"));
    lines << QString("- 记忆文件：%1").arg(
        QFile::exists(dir + QStringLiteral("/memory.json")) ? QStringLiteral("已生成")
                                                           : QStringLiteral("尚未生成"));

    return {true, QStringLiteral("Solace 自检：\n") + lines.join('\n')};
}
