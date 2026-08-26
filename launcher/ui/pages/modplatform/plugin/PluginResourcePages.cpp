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

#include "ui/pages/modplatform/plugin/PluginResourcePages.h"

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "plugins/Plugin.h"
#include "plugins/PluginManager.h"
#include "ui/dialogs/ResourceDownloadDialog.h"

namespace ResourceDownload {
namespace PluginProvider {

namespace {

constexpr auto PAGE_ID_PREFIX = "plugin:";

ResourceProviderData prepare(Plugins::Plugin* plugin)
{
    return {
        .displayName = plugin->manifest().name,
        .icon = plugin->icon(),
        .id = pageId(plugin->id()),
        // Namespaced so plugins cannot share, or clobber, another provider's cache.
        .metaEntryBase = QString("PluginPacks/%1").arg(plugin->id()),
        .debugName = plugin->manifest().name,
    };
}

const ResourceAPI* apiFor(Plugins::Plugin* plugin)
{
    return APPLICATION->pluginManager()->api(plugin->id());
}

}  // namespace

QString pageId(const QString& pluginId)
{
    return PAGE_ID_PREFIX + pluginId;
}

QString pluginIdFromPageId(const QString& pageId)
{
    if (!pageId.startsWith(PAGE_ID_PREFIX))
        return {};
    return pageId.mid(static_cast<qsizetype>(qstrlen(PAGE_ID_PREFIX)));
}

ShaderPackResourcePage* createShaderPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin)
{
    return new ShaderPackResourcePage(dialog, instance, prepare(plugin), apiFor(plugin));
}

DataPackResourcePage* createDataPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin)
{
    return new DataPackResourcePage(dialog, instance, prepare(plugin), apiFor(plugin));
}

ResourcePackResourcePage* createResourcePackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin)
{
    return new ResourcePackResourcePage(dialog, instance, prepare(plugin), apiFor(plugin));
}

TexturePackResourcePage* createTexturePackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin)
{
    return new TexturePackResourcePage(dialog, instance, prepare(plugin), apiFor(plugin));
}

ModPage* createModPage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin)
{
    return new ModPage(dialog, instance, prepare(plugin), apiFor(plugin),
                       ModFilterWidget::create(&static_cast<MinecraftInstance&>(instance), true));
}

}  // namespace PluginProvider
}  // namespace ResourceDownload
