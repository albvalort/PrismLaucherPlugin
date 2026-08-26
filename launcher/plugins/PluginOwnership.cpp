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

#include "plugins/PluginOwnership.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include "Application.h"
#include "FileSystem.h"
#include "modplatform/packwiz/Packwiz.h"
#include "settings/SettingsObject.h"

namespace Plugins {
namespace Ownership {

namespace {

constexpr auto INDEX_FOLDER = ".index";
constexpr auto METADATA_SUFFIX = ".pw.toml";

QStringList instanceRoots()
{
    QStringList roots{ APPLICATION->settings()->get("InstanceDir").toString() };
    roots << APPLICATION->settings()->get("AdditionalInstanceDirs").toStringList();

    roots.removeAll(QString{});
    roots.removeDuplicates();
    return roots;
}

std::optional<OwnedResource> inspect(const QString& pluginId, const QDir& indexDir, const QFileInfo& entry, const QString& instanceId)
{
    auto slug = entry.fileName();
    slug.chop(static_cast<qsizetype>(qstrlen(METADATA_SUFFIX)));

    // Reuse the real parser rather than a second, simplified reader of the same format.
    const auto mod = Packwiz::V1::getIndexForMod(indexDir, slug);
    if (mod.provider != ModPlatform::ResourceProvider::PLUGIN || mod.plugin_id != pluginId)
        return {};

    OwnedResource owned;
    owned.instanceId = instanceId;
    owned.metadataPath = entry.absoluteFilePath();
    owned.name = mod.name.isEmpty() ? slug : mod.name;
    owned.slug = slug;

    // Resources sit one level above `.index`.
    if (!mod.filename.isEmpty()) {
        QDir resourceDir(indexDir);
        if (resourceDir.cdUp())
            owned.resourcePath = resourceDir.absoluteFilePath(mod.filename);
    }

    return owned;
}

}  // namespace

QList<OwnedResource> scan(const QString& pluginId)
{
    return scanRoots(pluginId, instanceRoots());
}

QList<OwnedResource> scanRoots(const QString& pluginId, const QStringList& roots)
{
    QList<OwnedResource> owned;
    if (pluginId.isEmpty())
        return owned;

    for (const auto& root : roots) {
        QDir rootDir(root);
        if (!rootDir.exists())
            continue;

        const auto instances = rootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const auto& instanceId : instances) {
            // Walking for index folders stays correct for resource kinds added later, and does
            // not need the instance to be loadable.
            QDirIterator iterator(rootDir.absoluteFilePath(instanceId), QStringList{ INDEX_FOLDER }, QDir::Dirs | QDir::NoDotAndDotDot,
                                  QDirIterator::Subdirectories);

            while (iterator.hasNext()) {
                QDir indexDir(iterator.next());
                const auto entries = indexDir.entryInfoList(QStringList{ QString("*%1").arg(METADATA_SUFFIX) }, QDir::Files);

                for (const auto& entry : entries) {
                    if (auto resource = inspect(pluginId, indexDir, entry, instanceId); resource.has_value())
                        owned.append(resource.value());
                }
            }
        }
    }

    return owned;
}

int apply(const QList<OwnedResource>& resources, UninstallPolicy policy)
{
    if (policy == UninstallPolicy::KeepResources)
        return 0;

    int changed = 0;
    for (const auto& resource : resources) {
        bool touched = false;

        if (policy == UninstallPolicy::DeleteResources && !resource.resourcePath.isEmpty() && QFileInfo::exists(resource.resourcePath)) {
            // Trash rather than unlink, so the user can still change their mind.
            if (FS::trash(resource.resourcePath) || QFile::remove(resource.resourcePath))
                touched = true;
            else
                qWarning() << "[Plugin] Could not remove" << resource.resourcePath;
        }

        if (QFile::remove(resource.metadataPath))
            touched = true;
        else
            qWarning() << "[Plugin] Could not remove metadata" << resource.metadataPath;

        if (touched)
            ++changed;
    }

    return changed;
}

}  // namespace Ownership
}  // namespace Plugins
