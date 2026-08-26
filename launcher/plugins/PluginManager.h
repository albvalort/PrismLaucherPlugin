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
#include <QList>
#include <QObject>
#include <QString>

#include <map>
#include <memory>

#include "plugins/Plugin.h"
#include "plugins/PluginOwnership.h"
#include "plugins/PluginResourceAPI.h"

namespace Plugins {

/* Owns every plugin the launcher knows about.
 *
 * Follows ThemeManager: a folder under the data directory is scanned at startup and each subfolder
 * with a valid manifest becomes an entry. Which ones run is a separate setting.
 */
class PluginManager : public QObject {
    Q_OBJECT

   public:
    PluginManager();
    ~PluginManager() override;

    void initialize();

    QDir pluginsFolder() const { return m_folder; }

    // Sorted by display name.
    QList<Plugin*> plugins() const;

    Plugin* plugin(const QString& id) const;

    QList<Plugin*> providersFor(ModPlatform::ResourceType type) const;

    // Owned here so an API outlives any one download dialog.
    const PluginResourceAPI* api(const QString& id) const;

    bool setEnabled(const QString& id, bool enabled);

    // Returns the id of the installed plugin, or an empty string on failure.
    QString installFromArchive(const QString& archivePath, QString& error);

    QList<OwnedResource> ownedResources(const QString& id) const;

    bool uninstall(const QString& id, UninstallPolicy policy, QString& error);

   signals:
    void pluginsChanged();

   private:
    void scanFolder();

    QStringList enabledIds() const;
    void storeEnabledIds(const QStringList& ids);

    void syncApi(Plugin* plugin);

    QDir m_folder{ "plugins" };
    std::map<QString, std::unique_ptr<Plugin>> m_plugins;
    std::map<QString, std::unique_ptr<PluginResourceAPI>> m_apis;
};

}  // namespace Plugins
