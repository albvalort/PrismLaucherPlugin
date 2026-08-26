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

#include "plugins/PluginResourceAPI.h"

#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#include "plugins/Plugin.h"

namespace Plugins {

const QString PluginResourceAPI::NORMALIZED_MARKER = QStringLiteral("__prism_normalized");

namespace {

QJsonArray toJsonArray(const QStringList& values)
{
    QJsonArray array;
    for (const auto& value : values)
        array.append(value);
    return array;
}

QStringList loaderNames(const std::optional<ModPlatform::ModLoaderTypes>& loaders)
{
    QStringList names;
    if (!loaders.has_value())
        return names;
    for (auto loader : ModPlatform::modLoaderTypesToList(loaders.value()))
        names << ModPlatform::getModLoaderAsString(loader);
    return names;
}

QStringList versionNames(const std::optional<std::vector<Version>>& versions)
{
    QStringList names;
    if (!versions.has_value())
        return names;
    for (const auto& version : versions.value())
        names << version.toString();
    return names;
}

ModPlatform::ModLoaderTypes loadersFromJson(const QJsonArray& array)
{
    ModPlatform::ModLoaderTypes loaders;
    for (const auto& entry : array) {
        const auto loader = ModPlatform::getModLoaderFromString(entry.toString().toLower());
        if (loader != ModPlatform::ModLoaderType::None)
            loaders |= loader;
    }
    return loaders;
}

QStringList stringsFromJson(const QJsonArray& array)
{
    QStringList values;
    for (const auto& entry : array) {
        if (entry.isString())
            values << entry.toString();
    }
    return values;
}

}  // namespace

PluginResourceAPI::PluginResourceAPI(Plugin* plugin) : m_plugin(plugin) {}

std::optional<QString> PluginResourceAPI::urlFromPlugin(const char* function, const QJsonValue& argument) const
{
    if (m_plugin == nullptr || !m_plugin->isEnabled())
        return {};

    QString error;
    const auto result = m_plugin->runtime()->callForString(function, argument, error);
    if (!result.has_value()) {
        qWarning() << "[Plugin]" << m_plugin->id() << ":" << function << "failed:" << error;
        return {};
    }

    const QUrl url(result.value());
    if (!m_plugin->isUrlAllowed(url)) {
        qWarning() << "[Plugin]" << m_plugin->id() << "tried to reach a URL outside its declared hosts:" << result.value();
        return {};
    }

    return result;
}

QJsonArray PluginResourceAPI::arrayFromPlugin(const char* function, const QJsonValue& argument) const
{
    if (m_plugin == nullptr || !m_plugin->isEnabled())
        return {};

    QString error;
    const auto result = m_plugin->runtime()->callForJson(function, argument, error);
    if (!result.has_value()) {
        qWarning() << "[Plugin]" << m_plugin->id() << ":" << function << "failed:" << error;
        return {};
    }
    if (!result->isArray()) {
        qWarning() << "[Plugin]" << m_plugin->id() << ":" << function << "did not return a list.";
        return {};
    }
    return result->toArray();
}

QList<ResourceAPI::SortingMethod> PluginResourceAPI::getSortingMethods() const
{
    QList<SortingMethod> methods;

    if (m_plugin != nullptr && m_plugin->isEnabled() && m_plugin->runtime()->hasFunction("sortings")) {
        QString error;
        const auto result = m_plugin->runtime()->callForJson("sortings", QJsonValue(QJsonValue::Null), error);
        if (result.has_value() && result->isArray()) {
            unsigned int index = 1;
            for (const auto& entry : result->toArray()) {
                const auto object = entry.toObject();
                const auto id = object["id"].toString();
                if (id.isEmpty())
                    continue;
                methods.append({ .index = index++, .name = id, .readable_name = object["name"].toString(id) });
            }
        } else if (!result.has_value()) {
            qWarning() << "[Plugin]" << m_plugin->id() << ": sortings failed:" << error;
        }
    }

    // The sorting combo box is populated from this list, so it must never be empty.
    if (methods.isEmpty())
        methods.append({ .index = 1, .name = QStringLiteral("relevance"), .readable_name = QObject::tr("Sort by Relevance") });

    return methods;
}

std::optional<QString> PluginResourceAPI::getSearchURL(const SearchArgs& args) const
{
    QJsonObject request{
        { "query", args.search.value_or(QString{}) },
        { "offset", args.offset },
        { "resource_type", resourceTypeToString(args.type) },
        { "loaders", toJsonArray(loaderNames(args.loaders)) },
        { "game_versions", toJsonArray(versionNames(args.versions)) },
        { "categories", toJsonArray(args.categoryIds.value_or(QStringList{})) },
        { "open_source", args.openSource },
    };
    if (args.sorting.has_value())
        request.insert("sort", args.sorting->name);
    if (args.side.has_value() && args.side->isValid())
        request.insert("side", args.side->toString());

    return urlFromPlugin("search_url", request);
}

std::optional<QString> PluginResourceAPI::getInfoURL(const QString& id) const
{
    if (m_plugin == nullptr || !m_plugin->isEnabled() || !m_plugin->runtime()->hasFunction("project_url"))
        return {};

    return urlFromPlugin("project_url", QJsonObject{ { "project_id", id } });
}

std::optional<QString> PluginResourceAPI::getVersionsURL(const VersionSearchArgs& args) const
{
    const QJsonObject request{
        { "project_id", args.pack->addonId.toString() },
        { "resource_type", resourceTypeToString(args.resourceType) },
        { "loaders", toJsonArray(loaderNames(args.loaders)) },
        { "game_versions", toJsonArray(versionNames(args.mcVersions)) },
    };

    return urlFromPlugin("versions_url", request);
}

std::optional<QString> PluginResourceAPI::getDependencyURL(const DependencySearchArgs& args) const
{
    // The v1 API does not model dependencies. Returning nothing skips the lookup.
    Q_UNUSED(args);
    return {};
}

QJsonArray PluginResourceAPI::documentToArray(QJsonDocument& doc) const
{
    auto results = arrayFromPlugin("parse_search", QJsonValue::fromVariant(doc.toVariant()));

    QJsonArray marked;
    for (const auto& entry : std::as_const(results)) {
        if (!entry.isObject())
            continue;
        auto object = entry.toObject();
        object.insert(NORMALIZED_MARKER, true);
        marked.append(object);
    }
    return marked;
}

QJsonArray PluginResourceAPI::versionsDocumentToArray(const QJsonDocument& doc) const
{
    return arrayFromPlugin("parse_versions", QJsonValue::fromVariant(doc.toVariant()));
}

void PluginResourceAPI::loadIndexedPack(ModPlatform::IndexedPack& pack, QJsonObject& obj) const
{
    // Search results arrive normalised from documentToArray; a project-info response does not.
    QJsonObject source = obj;
    if (!obj.contains(NORMALIZED_MARKER) && m_plugin != nullptr && m_plugin->isEnabled() &&
        m_plugin->runtime()->hasFunction("parse_project")) {
        QString error;
        const auto parsed = m_plugin->runtime()->callForJson("parse_project", obj, error);
        if (parsed.has_value() && parsed->isObject())
            source = parsed->toObject();
        else if (!parsed.has_value())
            qWarning() << "[Plugin]" << m_plugin->id() << ": parse_project failed:" << error;
    }

    pack.provider = ModPlatform::ResourceProvider::PLUGIN;
    pack.pluginId = m_plugin != nullptr ? m_plugin->id() : QString{};

    pack.addonId = source["id"].toString();
    pack.name = source["name"].toString();
    pack.slug = source["slug"].toString(pack.addonId.toString());
    pack.description = source["description"].toString();
    pack.websiteUrl = source["website_url"].toString();

    pack.logoUrl = source["icon_url"].toString();
    // Keyed on in the icon cache, so it has to differ per pack and stay stable.
    pack.logoName = QString("%1.%2").arg(pack.slug, QFileInfo(QUrl(pack.logoUrl).fileName()).suffix());

    const auto author = source["author"].toString();
    if (!author.isEmpty())
        pack.authors = { ModPlatform::ModpackAuthor{ .name = author, .url = source["author_url"].toString() } };

    const auto side = source["side"].toString();
    if (!side.isEmpty())
        pack.side = ModPlatform::SideType::fromString(side);

    // Otherwise the UI waits on a request that will never be made.
    const bool canLoadExtra = m_plugin != nullptr && m_plugin->isEnabled() && m_plugin->runtime()->hasFunction("project_url") &&
                              !pack.addonId.toString().isEmpty();
    pack.extraDataLoaded = !canLoadExtra;

    if (source.contains("body"))
        loadExtraPackInfo(pack, source);
}

void PluginResourceAPI::loadExtraPackInfo(ModPlatform::IndexedPack& pack, QJsonObject& obj) const
{
    pack.extraData.body = obj["body"].toString();
    pack.extraData.sourceUrl = obj["source_url"].toString();
    pack.extraData.issuesUrl = obj["issues_url"].toString();
    pack.extraData.wikiUrl = obj["wiki_url"].toString();
    pack.extraData.discordUrl = obj["discord_url"].toString();
    pack.extraData.status = obj["status"].toString();
    pack.extraDataLoaded = true;
}

ModPlatform::IndexedVersion PluginResourceAPI::loadIndexedPackVersion(QJsonObject& obj, ModPlatform::ResourceType type) const
{
    Q_UNUSED(type);

    if (m_plugin == nullptr)
        return {};

    ModPlatform::IndexedVersion version;

    version.addonId = obj["project_id"].toString();
    version.fileId = obj["id"].toString();
    version.version = obj["name"].toString();
    version.version_number = obj["version_number"].toString(version.version);
    version.version_type = ModPlatform::IndexedVersionType::fromString(obj["release_type"].toString());
    version.downloadUrl = obj["download_url"].toString();
    version.fileName = obj["filename"].toString();
    version.date = obj["date"].toString();
    version.changelog = obj["changelog"].toString();
    version.hash = obj["hash"].toString();
    version.hash_type = obj["hash_format"].toString();
    version.mcVersion = stringsFromJson(obj["game_versions"].toArray());
    version.loaders = loadersFromJson(obj["loaders"].toArray());

    const auto side = obj["side"].toString();
    if (!side.isEmpty())
        version.side = ModPlatform::SideType::fromString(side);

    // Without a hash there is nothing to verify the download against.
    if (version.hash.isEmpty() || version.hash_type.isEmpty()) {
        qWarning() << "[Plugin]" << m_plugin->id() << "returned version" << version.version << "without a hash; ignoring it.";
        return {};
    }

    if (!m_plugin->isUrlAllowed(QUrl(version.downloadUrl))) {
        qWarning() << "[Plugin]" << m_plugin->id() << "returned a download URL outside its declared hosts:" << version.downloadUrl;
        return {};
    }

    return version;
}

std::pair<Task::Ptr, QByteArray*> PluginResourceAPI::getProjects(QStringList addonIds) const
{
    // Only used to rebuild metadata from file hashes, which plugins cannot serve.
    Q_UNUSED(addonIds);
    return { nullptr, nullptr };
}

std::pair<Task::Ptr, QByteArray*> PluginResourceAPI::getModCategories() const
{
    // Callers treat a null task as "this provider has no categories".
    return { nullptr, nullptr };
}

QList<ModPlatform::Category> PluginResourceAPI::loadModCategories(const QByteArray& response) const
{
    Q_UNUSED(response);
    return {};
}

}  // namespace Plugins
