// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 The Solace Authors
// This file is part of Solace, licensed under the GNU GPL v3.0 or
// later. See the LICENSE file for details.

#pragma once
#include "AgentTool.h"

// v5.2 read-only diagnostics. Each tool is independent and stateless so the
// registry can dispatch them from a worker thread without locking.

class NetworkDiagnoseTool : public AgentTool
{
public:
    QString id() const override;
    QString description() const override;
    ToolResult run(const QJsonObject &args) const override;
};

class SystemInfoTool : public AgentTool
{
public:
    QString id() const override;
    QString description() const override;
    ToolResult run(const QJsonObject &args) const override;
};

class ProcessListTool : public AgentTool
{
public:
    QString id() const override;
    QString description() const override;
    ToolResult run(const QJsonObject &args) const override;
};

class SelfCheckTool : public AgentTool
{
public:
    QString id() const override;
    QString description() const override;
    ToolResult run(const QJsonObject &args) const override;
};
