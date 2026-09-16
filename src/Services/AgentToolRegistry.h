// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QList>
#include <QString>
#include "AgentTool.h"

// v5.2: single place that owns the read-only agent tools, tells the model what
// exists (catalogText) and dispatches a call by id. Registration order is the
// order the model sees them in.
class AgentToolRegistry
{
public:
    static AgentToolRegistry &instance();

    QString catalogText() const;                       // prompt-ready tool list
    bool has(const QString &id) const;
    ToolResult run(const QString &id, const QJsonObject &args) const; // worker thread

private:
    AgentToolRegistry();
    QList<AgentTool *> m_tools;
};
