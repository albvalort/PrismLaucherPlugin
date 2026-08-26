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

#include "plugins/LuaRuntime.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QUrl>

#include <cstdlib>
#include <cstring>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace Plugins {

namespace {

constexpr int WATCHDOG_INTERVAL = 10'000;

// Also stops self-referential tables, which have no cheap identity set to track visited nodes.
constexpr int MAX_DEPTH = 32;

LuaRuntime* runtimeFor(lua_State* state)
{
    void* opaque = nullptr;
    lua_getallocf(state, &opaque);
    return static_cast<LuaRuntime*>(opaque);
}

void pushJson(lua_State* state, const QJsonValue& value, int depth);

void pushJsonObject(lua_State* state, const QJsonObject& object, int depth)
{
    lua_createtable(state, 0, static_cast<int>(object.size()));
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        // A nil value would just erase the key.
        if (it.value().isNull() || it.value().isUndefined())
            continue;
        const auto key = it.key().toUtf8();
        lua_pushlstring(state, key.constData(), static_cast<std::size_t>(key.size()));
        pushJson(state, it.value(), depth + 1);
        lua_rawset(state, -3);
    }
}

void pushJsonArray(lua_State* state, const QJsonArray& array, int depth)
{
    lua_createtable(state, static_cast<int>(array.size()), 0);
    lua_Integer index = 1;
    for (const auto& entry : array) {
        pushJson(state, entry, depth + 1);
        lua_rawseti(state, -2, index++);
    }
}

void pushJson(lua_State* state, const QJsonValue& value, int depth)
{
    if (depth > MAX_DEPTH) {
        lua_pushnil(state);
        return;
    }

    switch (value.type()) {
        case QJsonValue::Bool:
            lua_pushboolean(state, value.toBool() ? 1 : 0);
            break;
        case QJsonValue::Double:
            lua_pushnumber(state, value.toDouble());
            break;
        case QJsonValue::String: {
            const auto text = value.toString().toUtf8();
            lua_pushlstring(state, text.constData(), static_cast<std::size_t>(text.size()));
            break;
        }
        case QJsonValue::Array:
            pushJsonArray(state, value.toArray(), depth);
            break;
        case QJsonValue::Object:
            pushJsonObject(state, value.toObject(), depth);
            break;
        case QJsonValue::Null:
        case QJsonValue::Undefined:
        default:
            lua_pushnil(state);
            break;
    }
}

QJsonValue toJson(lua_State* state, int index, int depth, bool& ok);

// An array only when its integer-keyed sequence covers every key. Empty tables count as arrays.
bool tableIsArray(lua_State* state, int index)
{
    const auto sequenceLength = lua_rawlen(state, index);

    lua_Unsigned total = 0;
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        ++total;
        lua_pop(state, 1);
    }

    return total == sequenceLength;
}

QJsonValue tableToJson(lua_State* state, int index, int depth, bool& ok)
{
    if (tableIsArray(state, index)) {
        QJsonArray array;
        const auto length = lua_rawlen(state, index);
        for (lua_Unsigned i = 1; i <= length; ++i) {
            lua_rawgeti(state, index, static_cast<lua_Integer>(i));
            array.append(toJson(state, lua_gettop(state), depth + 1, ok));
            lua_pop(state, 1);
            if (!ok)
                return {};
        }
        return array;
    }

    QJsonObject object;
    lua_pushnil(state);
    while (lua_next(state, index) != 0) {
        // lua_tolstring on a number key in place would rewrite it and derail lua_next.
        if (lua_type(state, -2) == LUA_TSTRING) {
            std::size_t keyLength = 0;
            const char* keyData = lua_tolstring(state, -2, &keyLength);
            const auto key = QString::fromUtf8(keyData, static_cast<qsizetype>(keyLength));
            object.insert(key, toJson(state, lua_gettop(state), depth + 1, ok));
            if (!ok) {
                lua_pop(state, 2);
                return {};
            }
        }
        lua_pop(state, 1);
    }
    return object;
}

QJsonValue toJson(lua_State* state, int index, int depth, bool& ok)
{
    if (depth > MAX_DEPTH) {
        ok = false;
        return {};
    }

    switch (lua_type(state, index)) {
        case LUA_TNIL:
        case LUA_TNONE:
            return QJsonValue(QJsonValue::Null);
        case LUA_TBOOLEAN:
            return QJsonValue(lua_toboolean(state, index) != 0);
        case LUA_TNUMBER:
            return QJsonValue(lua_tonumber(state, index));
        case LUA_TSTRING: {
            std::size_t length = 0;
            const char* data = lua_tolstring(state, index, &length);
            return QJsonValue(QString::fromUtf8(data, static_cast<qsizetype>(length)));
        }
        case LUA_TTABLE:
            return tableToJson(state, index, depth, ok);
        default:
            ok = false;
            return {};
    }
}

int prismLog(lua_State* state)
{
    const char* message = luaL_optstring(state, 1, "");
    auto* runtime = runtimeFor(state);
    qDebug() << "[Plugin]" << (runtime != nullptr ? runtime->logName() : QStringLiteral("?")) << ":" << message;
    return 0;
}

int prismUrlEncode(lua_State* state)
{
    std::size_t length = 0;
    const char* data = luaL_checklstring(state, 1, &length);
    const auto encoded = QUrl::toPercentEncoding(QString::fromUtf8(data, static_cast<qsizetype>(length)));
    lua_pushlstring(state, encoded.constData(), static_cast<std::size_t>(encoded.size()));
    return 1;
}

const luaL_Reg PRISM_FUNCTIONS[] = { { "log", prismLog }, { "url_encode", prismUrlEncode }, { nullptr, nullptr } };

const char* const FORBIDDEN_GLOBALS[] = { "load", "loadfile", "dofile", "loadstring", "require", "collectgarbage", "print", "dostring" };

void watchdog(lua_State* state, lua_Debug* /*activation*/)
{
    auto* runtime = runtimeFor(state);
    if (runtime == nullptr)
        return;

    if (runtime->tick())
        return;

    luaL_error(state, "plugin exceeded its execution budget");
}

}  // namespace

LuaRuntime::LuaRuntime(QString logName, LuaLimits limits) : m_logName(std::move(logName)), m_limits(limits)
{
#if LUA_VERSION_NUM >= 505
    m_state = lua_newstate(&LuaRuntime::allocate, this, QRandomGenerator::global()->generate());
#else
    m_state = lua_newstate(&LuaRuntime::allocate, this);
#endif
    if (m_state == nullptr) {
        qWarning() << "[Plugin]" << m_logName << ": could not create a Lua state";
        return;
    }

    installSandbox();
}

LuaRuntime::~LuaRuntime()
{
    if (m_state != nullptr)
        lua_close(m_state);
}

void* LuaRuntime::allocate(void* opaque, void* pointer, std::size_t oldSize, std::size_t newSize)
{
    auto* runtime = static_cast<LuaRuntime*>(opaque);

    if (newSize == 0) {
        std::free(pointer);
        runtime->m_allocated -= oldSize;
        return nullptr;
    }

    // Lua treats a null return as a memory error and unwinds cleanly.
    if (newSize > oldSize && runtime->m_allocated + (newSize - oldSize) > runtime->m_limits.memoryBytes)
        return nullptr;

    void* result = std::realloc(pointer, newSize);
    if (result == nullptr)
        return nullptr;

    runtime->m_allocated += newSize - oldSize;
    return result;
}

bool LuaRuntime::tick()
{
    m_instructionsUsed += WATCHDOG_INTERVAL;

    if (m_budgetTripped)
        return false;

    // Instructions do not bound a script blocked in a long C call; the clock does not bound one
    // that spins cheaply. Both have to hold.
    if (m_instructionsUsed > m_limits.instructionLimit || m_callTimer.elapsed() > m_limits.wallClockMs) {
        // Latch it: a plugin can pcall away the error we raise, but the next check fires again.
        m_budgetTripped = true;
        return false;
    }

    return true;
}

void LuaRuntime::installSandbox()
{
    // Opt in rather than deleting afterwards, so anything a future Lua adds is absent by default.
    static const luaL_Reg SAFE_LIBRARIES[] = {
        { LUA_GNAME, luaopen_base },
        { LUA_STRLIBNAME, luaopen_string },
        { LUA_TABLIBNAME, luaopen_table },
        { LUA_MATHLIBNAME, luaopen_math },
    };

    for (const auto& library : SAFE_LIBRARIES) {
        luaL_requiref(m_state, library.name, library.func, 1);
        lua_pop(m_state, 1);
    }

    for (const char* name : FORBIDDEN_GLOBALS) {
        lua_pushnil(m_state);
        lua_setglobal(m_state, name);
    }

    lua_newtable(m_state);
    luaL_setfuncs(m_state, PRISM_FUNCTIONS, 0);
    lua_setglobal(m_state, "prism");

    lua_sethook(m_state, &watchdog, LUA_MASKCOUNT, WATCHDOG_INTERVAL);
}

void LuaRuntime::beginCall()
{
    m_instructionsUsed = 0;
    m_budgetTripped = false;
    m_callTimer.start();
}

bool LuaRuntime::loadScript(const QString& path, QString& error)
{
    if (!isValid()) {
        error = QObject::tr("The plugin sandbox failed to start.");
        return false;
    }

    // "t" refuses precompiled chunks: the bytecode verifier is not hardened against hostile input.
    if (luaL_loadfilex(m_state, path.toUtf8().constData(), "t") != LUA_OK) {
        error = QString::fromUtf8(lua_tostring(m_state, -1));
        lua_pop(m_state, 1);
        return false;
    }

    beginCall();
    if (lua_pcall(m_state, 0, 0, 0) != LUA_OK) {
        error = QString::fromUtf8(lua_tostring(m_state, -1));
        lua_pop(m_state, 1);
        return false;
    }

    return true;
}

bool LuaRuntime::hasFunction(const char* name) const
{
    if (!isValid())
        return false;

    lua_getglobal(m_state, name);
    const bool callable = lua_isfunction(m_state, -1);
    lua_pop(m_state, 1);
    return callable;
}

bool LuaRuntime::prepareCall(const char* name, const QJsonValue& argument, QString& error)
{
    lua_getglobal(m_state, name);
    if (!lua_isfunction(m_state, -1)) {
        lua_pop(m_state, 1);
        error = QObject::tr("The plugin does not define a function called \"%1\".").arg(QString::fromUtf8(name));
        return false;
    }

    pushJson(m_state, argument, 0);
    beginCall();
    return true;
}

std::optional<QString> LuaRuntime::callForString(const char* name, const QJsonValue& argument, QString& error)
{
    if (!isValid()) {
        error = QObject::tr("The plugin sandbox failed to start.");
        return {};
    }
    if (!prepareCall(name, argument, error))
        return {};

    if (lua_pcall(m_state, 1, 1, 0) != LUA_OK) {
        error = QString::fromUtf8(lua_tostring(m_state, -1));
        lua_pop(m_state, 1);
        return {};
    }

    if (lua_type(m_state, -1) != LUA_TSTRING) {
        lua_pop(m_state, 1);
        error = QObject::tr("\"%1\" was expected to return a string.").arg(QString::fromUtf8(name));
        return {};
    }

    std::size_t length = 0;
    const char* data = lua_tolstring(m_state, -1, &length);
    const auto result = QString::fromUtf8(data, static_cast<qsizetype>(length));
    lua_pop(m_state, 1);
    return result;
}

std::optional<QJsonValue> LuaRuntime::callForJson(const char* name, const QJsonValue& argument, QString& error)
{
    if (!isValid()) {
        error = QObject::tr("The plugin sandbox failed to start.");
        return {};
    }
    if (!prepareCall(name, argument, error))
        return {};

    if (lua_pcall(m_state, 1, 1, 0) != LUA_OK) {
        error = QString::fromUtf8(lua_tostring(m_state, -1));
        lua_pop(m_state, 1);
        return {};
    }

    if (lua_type(m_state, -1) != LUA_TTABLE) {
        lua_pop(m_state, 1);
        error = QObject::tr("\"%1\" was expected to return a table.").arg(QString::fromUtf8(name));
        return {};
    }

    bool ok = true;
    auto result = toJson(m_state, lua_gettop(m_state), 0, ok);
    lua_pop(m_state, 1);

    if (!ok) {
        error = QObject::tr("\"%1\" returned a value that cannot be represented as JSON.").arg(QString::fromUtf8(name));
        return {};
    }
    return result;
}

bool LuaRuntime::callOptionalHook(const char* name, QString& error)
{
    if (!isValid() || !hasFunction(name))
        return true;

    lua_getglobal(m_state, name);
    beginCall();

    if (lua_pcall(m_state, 0, 0, 0) != LUA_OK) {
        error = QString::fromUtf8(lua_tostring(m_state, -1));
        lua_pop(m_state, 1);
        return false;
    }
    return true;
}

}  // namespace Plugins
