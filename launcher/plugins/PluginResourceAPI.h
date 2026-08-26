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

#include "modplatform/ResourceAPI.h"

namespace Plugins {

class Plugin;

/* Exposes a plugin as a resource provider.
 *
 * ResourceAPI asks its subclasses for URLs and for help reshaping responses; the launcher does the
 * networking. Keeping that shape is what lets a plugin stay sandboxed - it never sees a socket,
 * only JSON that already arrived.
 *
 * parse_search and parse_versions normalise a whole document in one call; the per-entry loaders
 * below just read the normalised fields back out.
 */
class PluginResourceAPI final : public ResourceAPI {
   public:
    explicit PluginResourceAPI(Plugin* plugin);
    ~PluginResourceAPI() = default;

    QList<SortingMethod> getSortingMethods() const override;

    std::optional<QString> getSearchURL(const SearchArgs& args) const override;
    std::optional<QString> getInfoURL(const QString& id) const override;
    std::optional<QString> getVersionsURL(const VersionSearchArgs& args) const override;
    std::optional<QString> getDependencyURL(const DependencySearchArgs& args) const override;

    void loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj) const override;
    ModPlatform::IndexedVersion loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType type) const override;
    void loadExtraPackInfo(ModPlatform::IndexedPack& pack, QJsonObject& obj) const override;

    QJsonArray documentToArray(QJsonDocument& doc) const override;
    QJsonArray versionsDocumentToArray(const QJsonDocument& doc) const override;

    std::pair<Task::Ptr, QByteArray*> getProjects(QStringList addonIds) const override;
    std::pair<Task::Ptr, QByteArray*> getModCategories() const override;
    QList<ModPlatform::Category> loadModCategories(const QByteArray& response) const override;

    Plugin* plugin() const { return m_plugin; }

   private:
    // Refuses anything outside the manifest allowlist.
    std::optional<QString> urlFromPlugin(const char* function, const QJsonValue& argument) const;

    // Plugin errors come back as an empty array rather than an exception.
    QJsonArray arrayFromPlugin(const char* function, const QJsonValue& argument) const;

    // Marks entries this class has already normalised.
    static const QString NORMALIZED_MARKER;

    Plugin* m_plugin;
};

}  // namespace Plugins
