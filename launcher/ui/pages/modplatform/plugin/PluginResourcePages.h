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

#include <QString>

#include "ui/pages/modplatform/DataPackPage.h"
#include "ui/pages/modplatform/ModPage.h"
#include "ui/pages/modplatform/ResourcePackPage.h"
#include "ui/pages/modplatform/ShaderPackPage.h"
#include "ui/pages/modplatform/TexturePackPage.h"

namespace Plugins {
class Plugin;
}

namespace ResourceDownload {

/* Download-dialog pages backed by a plugin.
 *
 * ResourcePage and its subclasses are already parameterised over a ResourceProviderData and a
 * ResourceAPI, so a plugin supplies those two and needs no pages or .ui files of its own.
 */
namespace PluginProvider {

// Namespaced so a plugin cannot claim a built-in provider's id.
QString pageId(const QString& pluginId);

QString pluginIdFromPageId(const QString& pageId);

ShaderPackResourcePage* createShaderPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin);
DataPackResourcePage* createDataPackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin);
ResourcePackResourcePage* createResourcePackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin);
TexturePackResourcePage* createTexturePackResourcePage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin);
ModPage* createModPage(ResourceDownloadDialog* dialog, BaseInstance& instance, Plugins::Plugin* plugin);

}  // namespace PluginProvider

}  // namespace ResourceDownload
