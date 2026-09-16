// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#include "AgentToolRegistry.h"
#include "AgentTools.h"
#include <QStringList>

AgentToolRegistry &AgentToolRegistry::instance()
{
    static AgentToolRegistry registry;
    return registry;
}

AgentToolRegistry::AgentToolRegistry()
{
    m_tools << new NetworkDiagnoseTool
            << new SystemInfoTool
            << new ProcessListTool
            << new SelfCheckTool;
}

QString AgentToolRegistry::catalogText() const
{
    QStringList lines;
    for (const AgentTool *tool : m_tools)
        lines << QString("- %1：%2").arg(tool->id(), tool->description());
    return lines.join('\n');
}

bool AgentToolRegistry::has(const QString &id) const
{
    for (const AgentTool *tool : m_tools) {
        if (tool->id() == id)
            return true;
    }
    return false;
}

ToolResult AgentToolRegistry::run(const QString &id, const QJsonObject &args) const
{
    for (const AgentTool *tool : m_tools) {
        if (tool->id() == id)
            return tool->run(args);
    }
    return {false, QStringLiteral("未知工具：%1").arg(id)};
}
