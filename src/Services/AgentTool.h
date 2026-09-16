// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include <QJsonObject>
#include <QString>

// v5.2 Agent Tool: a read-only capability the companion may invoke when the
// user asks about live machine state (network / system / processes / self).
//
// Contract: run() executes on a worker thread and must never touch the GUI.
struct ToolResult {
    bool ok = false;
    QString text;   // prompt-ready plain text; no markdown tables, no secrets
};

class AgentTool
{
public:
    virtual ~AgentTool() = default;
    virtual QString id() const = 0;           // stable id used by the model
    virtual QString description() const = 0;  // one line, shown in the catalog
    virtual ToolResult run(const QJsonObject &args) const = 0;
};
