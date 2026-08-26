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

#include <QDir>
#include <QIcon>
#include <QString>
#include <QUrl>

#include <memory>

#include "plugins/LuaRuntime.h"
#include "plugins/PluginManifest.h"

namespace Plugins {

class PluginResourceAPI;

struct MenuItem {
    QString id;
    QString label;
    bool separator{ false };
};

// What the host should do when a menu entry is chosen. Plugins describe, the launcher decides.
struct MenuAction {
    enum class Type { None, Message, OpenUrl };

    Type type{ Type::None };
    QString title;
    QString text;
    QUrl url;
};

/* One plugin on disk, plus the interpreter it runs in when enabled.
 *
 * A Plugin exists for every plugin folder found, enabled or not, so the manager can list and
 * uninstall one without ever executing its code.
 */
class Plugin {
   public:
    enum class State { Disabled, Enabled, Broken };

    Plugin(Manifest manifest, QDir folder);
    ~Plugin();

    Plugin(const Plugin&) = delete;
    Plugin& operator=(const Plugin&) = delete;

    const Manifest& manifest() const { return m_manifest; }
    const QString& id() const { return m_manifest.id; }
    const QDir& folder() const { return m_folder; }

    State state() const { return m_state; }
    bool isEnabled() const { return m_state == State::Enabled; }
    const QString& errorString() const { return m_error; }

    bool enable();
    void disable();

    // Best effort: the host never relies on this to keep instances consistent.
    void notifyUninstall();

    LuaRuntime* runtime() { return m_runtime.get(); }

    bool providesResources() const;
    bool providesMenu() const;

    // Queried on every open so a plugin can reflect its own state.
    QList<MenuItem> menuItems();
    MenuAction activateMenuItem(const QString& itemId);

    bool isUrlAllowed(const QUrl& url) const;

    QIcon icon() const;

   private:
    Manifest m_manifest;
    QDir m_folder;
    State m_state{ State::Disabled };
    QString m_error;
    std::unique_ptr<LuaRuntime> m_runtime;
};

}  // namespace Plugins
