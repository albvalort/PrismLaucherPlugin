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

#include "plugins/PluginManager.h"

#include <QDebug>
#include <QDirIterator>
#include <QFileInfo>
#include <QTemporaryDir>

#include <algorithm>

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "settings/SettingsObject.h"

namespace Plugins {

namespace {

constexpr auto MANIFEST_NAME = "plugin.json";
constexpr auto ENABLED_SETTING = "EnabledPlugins";

}  // namespace

PluginManager::PluginManager() = default;

PluginManager::~PluginManager()
{
    // APIs point at plugins, so they go first.
    m_apis.clear();
    m_plugins.clear();
}

QStringList PluginManager::enabledIds() const
{
    return APPLICATION->settings()->get(ENABLED_SETTING).toStringList();
}

void PluginManager::storeEnabledIds(const QStringList& ids)
{
    APPLICATION->settings()->set(ENABLED_SETTING, ids);
}

void PluginManager::initialize()
{
    if (!m_folder.mkpath("."))
        qWarning() << "[Plugin] Could not create the plugins folder";
    qDebug() << "[Plugin] Plugins folder:" << m_folder.absolutePath();

    scanFolder();

    // After the whole folder is known, so one failure cannot hide the rest of the list.
    for (const auto& id : enabledIds()) {
        auto* found = plugin(id);
        if (found == nullptr) {
            qWarning() << "[Plugin]" << id << "is enabled in settings but is no longer installed";
            continue;
        }
        if (!found->enable())
            qWarning() << "[Plugin] Could not enable" << id << ":" << found->errorString();
        syncApi(found);
    }

    emit pluginsChanged();
}

void PluginManager::scanFolder()
{
    m_apis.clear();
    m_plugins.clear();

    QDirIterator iterator(m_folder.path(), QDir::Dirs | QDir::NoDotAndDotDot);
    while (iterator.hasNext()) {
        QDir folder(iterator.next());
        const auto manifestPath = folder.absoluteFilePath(MANIFEST_NAME);
        if (!QFileInfo::exists(manifestPath))
            continue;

        QString error;
        auto manifest = parseManifest(manifestPath, error);
        if (!manifest.has_value()) {
            qWarning() << "[Plugin] Ignoring" << folder.dirName() << ":" << error;
            continue;
        }

        // Uninstall and the enabled list address plugins by folder name.
        if (manifest->id != folder.dirName()) {
            qWarning() << "[Plugin] Ignoring" << folder.dirName() << ": its manifest declares the id" << manifest->id;
            continue;
        }

        m_plugins.emplace(manifest->id, std::make_unique<Plugin>(manifest.value(), folder));
        qDebug() << "[Plugin] Found" << manifest->id << manifest->version;
    }
}

QList<Plugin*> PluginManager::plugins() const
{
    QList<Plugin*> result;
    result.reserve(static_cast<qsizetype>(m_plugins.size()));
    for (const auto& [id, plugin] : m_plugins)
        result.append(plugin.get());

    std::sort(result.begin(), result.end(),
              [](const Plugin* a, const Plugin* b) { return a->manifest().name.localeAwareCompare(b->manifest().name) < 0; });
    return result;
}

Plugin* PluginManager::plugin(const QString& id) const
{
    const auto found = m_plugins.find(id);
    return found == m_plugins.end() ? nullptr : found->second.get();
}

const PluginResourceAPI* PluginManager::api(const QString& id) const
{
    const auto found = m_apis.find(id);
    return found == m_apis.end() ? nullptr : found->second.get();
}

QList<Plugin*> PluginManager::providersFor(ModPlatform::ResourceType type) const
{
    QList<Plugin*> result;
    for (auto* candidate : plugins()) {
        if (candidate->providesResources() && candidate->manifest().supports(type))
            result.append(candidate);
    }
    return result;
}

void PluginManager::syncApi(Plugin* target)
{
    if (target == nullptr)
        return;

    if (target->providesResources()) {
        if (m_apis.find(target->id()) == m_apis.end())
            m_apis.emplace(target->id(), std::make_unique<PluginResourceAPI>(target));
    } else {
        m_apis.erase(target->id());
    }
}

bool PluginManager::setEnabled(const QString& id, bool enabled)
{
    auto* target = plugin(id);
    if (target == nullptr)
        return false;

    if (enabled) {
        if (!target->enable()) {
            qWarning() << "[Plugin] Could not enable" << id << ":" << target->errorString();
            syncApi(target);
            emit pluginsChanged();
            return false;
        }
    } else {
        target->disable();
    }

    syncApi(target);

    auto ids = enabledIds();
    ids.removeAll(id);
    if (enabled)
        ids.append(id);
    storeEnabledIds(ids);

    emit pluginsChanged();
    return true;
}

QString PluginManager::installFromArchive(const QString& archivePath, QString& error)
{
    QTemporaryDir staging;
    if (!staging.isValid()) {
        error = QObject::tr("Could not create a temporary folder to unpack into.");
        return {};
    }

    // Unpack somewhere disposable: a rejected archive must not have written itself in already.
    if (!MMCZip::extractDir(archivePath, staging.path()).has_value()) {
        error = QObject::tr("Could not unpack the archive.");
        return {};
    }

    QDir source(staging.path());
    if (!QFileInfo::exists(source.absoluteFilePath(MANIFEST_NAME))) {
        // Archives made with "compress this folder" put everything one level down.
        const auto entries = source.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        if (entries.size() == 1 && QFileInfo::exists(source.filePath(entries.first() + "/" + MANIFEST_NAME)))
            source.cd(entries.first());
    }

    const auto manifestPath = source.absoluteFilePath(MANIFEST_NAME);
    if (!QFileInfo::exists(manifestPath)) {
        error = QObject::tr("The archive does not contain a %1 file.").arg(MANIFEST_NAME);
        return {};
    }

    auto manifest = parseManifest(manifestPath, error);
    if (!manifest.has_value())
        return {};

    if (!QFileInfo::exists(source.absoluteFilePath(manifest->entry))) {
        error = QObject::tr("The archive does not contain the entry point \"%1\".").arg(manifest->entry);
        return {};
    }

    const auto target = m_folder.absoluteFilePath(manifest->id);
    if (QFileInfo::exists(target)) {
        error = QObject::tr("A plugin with the id \"%1\" is already installed. Remove it first.").arg(manifest->id);
        return {};
    }

    FS::copy install(source.absolutePath(), target);
    // An archive can contain a symlink pointing anywhere.
    install.followSymlinks(false);
    if (!install()) {
        error = QObject::tr("Could not copy the plugin into %1.").arg(m_folder.absolutePath());
        return {};
    }

    m_plugins.emplace(manifest->id, std::make_unique<Plugin>(manifest.value(), QDir(target)));
    emit pluginsChanged();

    // Left switched off: running third-party code should be a deliberate second step.
    return manifest->id;
}

QList<OwnedResource> PluginManager::ownedResources(const QString& id) const
{
    return Ownership::scan(id);
}

bool PluginManager::uninstall(const QString& id, UninstallPolicy policy, QString& error)
{
    auto* target = plugin(id);
    if (target == nullptr) {
        error = QObject::tr("No plugin with the id \"%1\" is installed.").arg(id);
        return false;
    }

    const auto folder = target->folder().absolutePath();

    // A courtesy: the cleanup below is what actually keeps instances consistent.
    target->notifyUninstall();

    const auto owned = Ownership::scan(id);
    const int changed = Ownership::apply(owned, policy);
    if (policy != UninstallPolicy::KeepResources)
        qDebug() << "[Plugin] Applied the uninstall policy to" << changed << "of" << owned.size() << "resources owned by" << id;
    else if (!owned.isEmpty())
        qDebug() << "[Plugin] Leaving" << owned.size() << "resources owned by" << id << "in place";

    m_apis.erase(id);
    m_plugins.erase(id);

    auto ids = enabledIds();
    ids.removeAll(id);
    storeEnabledIds(ids);

    if (!FS::deletePath(folder)) {
        error = QObject::tr("The plugin was removed from the launcher, but its folder at %1 could not be deleted.").arg(folder);
        emit pluginsChanged();
        return false;
    }

    emit pluginsChanged();
    return true;
}

}  // namespace Plugins
