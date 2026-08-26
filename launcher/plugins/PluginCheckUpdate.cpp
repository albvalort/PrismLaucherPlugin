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

#include "plugins/PluginCheckUpdate.h"

#include <QDebug>

#include "Application.h"
#include "ResourceDownloadTask.h"
#include "minecraft/mod/MetadataHandler.h"
#include "minecraft/mod/Resource.h"
#include "plugins/PluginManager.h"
#include "plugins/PluginResourceAPI.h"

namespace Plugins {

PluginCheckUpdate::PluginCheckUpdate(QList<Resource*>& resources,
                                     std::vector<Version>& mcVersions,
                                     QList<ModPlatform::ModLoaderType> loadersList,
                                     ResourceFolderModel* resourceModel)
    : CheckUpdateTask(resources, mcVersions, std::move(loadersList), resourceModel)
{}

bool PluginCheckUpdate::abort()
{
    if (m_currentJob)
        return m_currentJob->abort();
    return true;
}

void PluginCheckUpdate::executeTask()
{
    setStatus(tr("Checking for updates from plugins..."));

    for (auto* resource : m_resources) {
        if (resource->metadata() && resource->metadata()->provider == ModPlatform::ResourceProvider::PLUGIN)
            m_pending.append(resource);
    }

    setProgress(0, m_pending.size());
    checkNext();
}

void PluginCheckUpdate::checkNext()
{
    if (m_index >= m_pending.size()) {
        emitSucceeded();
        return;
    }

    auto* resource = m_pending.at(m_index);
    ++m_index;
    setProgress(m_index, m_pending.size());

    const auto& metadata = resource->metadata();
    const auto* api = APPLICATION->pluginManager()->api(metadata->plugin_id);
    if (api == nullptr) {
        // A silently skipped resource looks identical to one that is up to date.
        emit checkFailed(resource, tr("The plugin \"%1\" that manages this resource is not enabled.").arg(metadata->plugin_id));
        checkNext();
        return;
    }

    auto pack = std::make_shared<ModPlatform::IndexedPack>();
    pack->name = resource->name();
    pack->slug = metadata->slug;
    pack->addonId = metadata->project_id;
    pack->provider = ModPlatform::ResourceProvider::PLUGIN;
    pack->pluginId = metadata->plugin_id;

    ResourceAPI::VersionSearchArgs args{
        .pack = pack, .mcVersions = m_gameVersions, .loaders = {}, .resourceType = ModPlatform::ResourceType::Mod, .includeChangelog = true
    };
    if (!m_loadersList.isEmpty()) {
        ModPlatform::ModLoaderTypes loaders;
        for (auto loader : m_loadersList)
            loaders |= loader;
        args.loaders = loaders;
    }

    ResourceAPI::Callback<QVector<ModPlatform::IndexedVersion>> callbacks;
    callbacks.on_succeed = [this, resource](QVector<ModPlatform::IndexedVersion>& versions) {
        onVersions(resource, versions);
        checkNext();
    };
    callbacks.on_fail = [this, resource](const QString& reason, int) {
        emit checkFailed(resource, tr("Could not check for updates: %1").arg(reason));
        checkNext();
    };
    callbacks.on_abort = [this] { emitAborted(); };

    auto job = api->getProjectVersions(std::move(args), std::move(callbacks));
    if (!job) {
        emit checkFailed(resource, tr("The plugin \"%1\" could not build a version request.").arg(metadata->plugin_id));
        checkNext();
        return;
    }

    m_currentJob = job;
    job->start();
}

void PluginCheckUpdate::onVersions(Resource* resource, const QVector<ModPlatform::IndexedVersion>& versions)
{
    const auto& metadata = resource->metadata();

    // Newest first, already filtered for a usable download URL.
    const ModPlatform::IndexedVersion* candidate = nullptr;
    for (const auto& version : versions) {
        if (!m_loadersList.isEmpty() && version.loaders != 0) {
            bool matches = false;
            for (auto loader : m_loadersList)
                matches = matches || version.loaders.testFlag(loader);
            if (!matches)
                continue;
        }
        candidate = &version;
        break;
    }

    if (candidate == nullptr) {
        emit checkFailed(resource, tr("No valid version found for this resource. It's probably unavailable for the current game "
                                      "version / mod loader."));
        return;
    }

    // Version ids, not hashes: that is what the metadata records.
    if (candidate->fileId.toString() == metadata->file_id.toString() && resource->status() != ResourceStatus::NotInstalled)
        return;

    auto pack = std::make_shared<ModPlatform::IndexedPack>();
    pack->name = resource->name();
    pack->slug = metadata->slug;
    pack->addonId = metadata->project_id;
    pack->provider = ModPlatform::ResourceProvider::PLUGIN;
    pack->pluginId = metadata->plugin_id;

    auto version = *candidate;
    auto downloadTask = makeShared<ResourceDownloadTask>(pack, version, m_resourceModel, true, "update");

    QString oldVersion = metadata->version_number;
    if (oldVersion.isEmpty())
        oldVersion = resource->status() == ResourceStatus::NotInstalled ? tr("Not installed") : tr("Unknown");

    m_updates.emplace_back(pack->name, metadata->hash, oldVersion, version.version_number, version.version_type, version.changelog,
                           ModPlatform::ResourceProvider::PLUGIN, downloadTask, resource->enabled());
}

}  // namespace Plugins
