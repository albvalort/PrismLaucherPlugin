// SPDX-FileCopyrightText: 2026 albvalort <albvalort@outlook.es>
//
// SPDX-License-Identifier: GPL-3.0-only

/*
 *  Prism Launcher - Minecraft Launcher
 *  Copyright (C) 2026 albvalort <albvalort@outlook.es>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include <QElapsedTimer>
#include <QJsonValue>
#include <QString>

#include <cstddef>
#include <optional>

struct lua_State;

namespace Plugins {

struct LuaLimits {
    std::size_t memoryBytes{ 32 * 1024 * 1024 };
    qint64 instructionLimit{ 20'000'000 };
    qint64 wallClockMs{ 2'000 };
};

/* A sandboxed Lua interpreter.
 *
 * Only base (minus its loaders), string, table and math are registered, so there is no filesystem,
 * process or socket access. Networking is the host's job: plugins return URLs, the launcher fetches
 * them.
 */
class LuaRuntime {
   public:
    explicit LuaRuntime(QString logName, LuaLimits limits = {});
    ~LuaRuntime();

    LuaRuntime(const LuaRuntime&) = delete;
    LuaRuntime& operator=(const LuaRuntime&) = delete;
    LuaRuntime(LuaRuntime&&) = delete;
    LuaRuntime& operator=(LuaRuntime&&) = delete;

    bool isValid() const { return m_state != nullptr; }

    bool loadScript(const QString& path, QString& error);

    bool hasFunction(const char* name) const;

    std::optional<QString> callForString(const char* name, const QJsonValue& argument, QString& error);
    std::optional<QJsonValue> callForJson(const char* name, const QJsonValue& argument, QString& error);

    // Missing functions are not an error.
    bool callOptionalHook(const char* name, QString& error);

    const QString& logName() const { return m_logName; }

    /* Charges one watchdog interval against the current call. Public because the Lua count hook is
     * a plain C callback. Returns false once a budget is blown, and keeps returning false.
     */
    bool tick();

   private:
    static void* allocate(void* opaque, void* pointer, std::size_t oldSize, std::size_t newSize);

    void installSandbox();
    bool prepareCall(const char* name, const QJsonValue& argument, QString& error);
    void beginCall();

    lua_State* m_state{ nullptr };
    QString m_logName;
    LuaLimits m_limits;

    std::size_t m_allocated{ 0 };
    qint64 m_instructionsUsed{ 0 };
    bool m_budgetTripped{ false };
    QElapsedTimer m_callTimer;
};

}  // namespace Plugins
