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

#include "plugins/PluginManifest.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>

namespace Plugins {

namespace {

const QMap<QString, ModPlatform::ResourceType>& resourceTypeNames()
{
    static const QMap<QString, ModPlatform::ResourceType> names = {
        { "mod", ModPlatform::ResourceType::Mod },
        { "resourcepack", ModPlatform::ResourceType::ResourcePack },
        { "texturepack", ModPlatform::ResourceType::TexturePack },
        { "shaderpack", ModPlatform::ResourceType::ShaderPack },
        { "datapack", ModPlatform::ResourceType::DataPack },
    };
    return names;
}

bool isSafeEntryName(const QString& entry)
{
    if (entry.isEmpty() || !entry.endsWith(".lua"))
        return false;
    if (entry.contains('/') || entry.contains('\\') || entry.contains(".."))
        return false;
    return true;
}

// Bare hostname only: no scheme, port, path or wildcards.
bool isSafeHost(const QString& host)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9]([a-z0-9-]*[a-z0-9])?(\\.[a-z0-9]([a-z0-9-]*[a-z0-9])?)+$"));
    return pattern.match(host).hasMatch();
}

}  // namespace

QString resourceTypeToString(ModPlatform::ResourceType type)
{
    for (auto it = resourceTypeNames().constBegin(); it != resourceTypeNames().constEnd(); ++it) {
        if (it.value() == type)
            return it.key();
    }
    return QStringLiteral("unknown");
}

bool isValidId(const QString& id)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9][a-z0-9-]{1,63}$"));
    return pattern.match(id).hasMatch();
}

std::optional<Manifest> parseManifest(const QString& path, QString& error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QObject::tr("Could not open %1: %2").arg(path, file.errorString());
        return {};
    }

    QJsonParseError parseError{};
    const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        error = QObject::tr("Invalid JSON at offset %1: %2").arg(parseError.offset).arg(parseError.errorString());
        return {};
    }
    if (!doc.isObject()) {
        error = QObject::tr("The manifest must be a JSON object.");
        return {};
    }

    const auto root = doc.object();
    Manifest manifest;

    manifest.id = root.value("id").toString();
    if (!isValidId(manifest.id)) {
        error = QObject::tr("\"id\" must be 2-64 characters of a-z, 0-9 or '-', starting with a letter or digit.");
        return {};
    }

    manifest.name = root.value("name").toString();
    if (manifest.name.isEmpty())
        manifest.name = manifest.id;

    manifest.version = root.value("version").toString();
    manifest.author = root.value("author").toString();
    manifest.description = root.value("description").toString();

    const auto apiVersionValue = root.value("api_version");
    if (!apiVersionValue.isDouble()) {
        error = QObject::tr("\"api_version\" is missing or is not a number.");
        return {};
    }
    manifest.apiVersion = apiVersionValue.toInt();
    if (manifest.apiVersion != CURRENT_API_VERSION) {
        error = QObject::tr("This plugin targets API version %1, but this launcher only supports version %2.")
                    .arg(manifest.apiVersion)
                    .arg(CURRENT_API_VERSION);
        return {};
    }

    if (root.contains("entry"))
        manifest.entry = root.value("entry").toString();
    if (!isSafeEntryName(manifest.entry)) {
        error = QObject::tr("\"entry\" must be a plain .lua file name inside the plugin folder.");
        return {};
    }

    // An empty list is a hard failure rather than a permissive default.
    const auto hosts = root.value("hosts").toArray();
    if (hosts.isEmpty()) {
        error = QObject::tr("\"hosts\" must list at least one hostname the plugin is allowed to contact.");
        return {};
    }
    for (const auto& host : hosts) {
        const auto text = host.toString().toLower();
        if (!isSafeHost(text)) {
            error = QObject::tr("\"%1\" is not a valid hostname.").arg(host.toString());
            return {};
        }
        manifest.hosts << text;
    }

    const auto types = root.value("resource_types").toArray();
    for (const auto& type : types) {
        const auto text = type.toString().toLower();
        const auto found = resourceTypeNames().constFind(text);
        if (found == resourceTypeNames().constEnd()) {
            error = QObject::tr("\"%1\" is not a supported resource type.").arg(type.toString());
            return {};
        }
        if (!manifest.resourceTypes.contains(found.value()))
            manifest.resourceTypes << found.value();
    }
    if (manifest.resourceTypes.isEmpty())
        manifest.resourceTypes << ModPlatform::ResourceType::Mod;

    return manifest;
}

}  // namespace Plugins
