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
#include <QStringList>

#include <optional>

#include "modplatform/ResourceType.h"

namespace Plugins {

constexpr int CURRENT_API_VERSION = 1;

struct Manifest {
    // Also used as the folder name and as the owner tag written into mod metadata.
    QString id;
    QString name;
    QString version;
    QString author;
    QString description;
    QString entry{ "main.lua" };
    int apiVersion{ 0 };
    // Hosts the plugin may reach. Every URL it returns is checked against this.
    QStringList hosts;
    QList<ModPlatform::ResourceType> resourceTypes;

    bool supports(ModPlatform::ResourceType type) const { return resourceTypes.contains(type); }
};

std::optional<Manifest> parseManifest(const QString& path, QString& error);

// Restricted to [a-z0-9-] so an id is safe as a folder name and as a TOML key.
bool isValidId(const QString& id);

QString resourceTypeToString(ModPlatform::ResourceType type);

}  // namespace Plugins
