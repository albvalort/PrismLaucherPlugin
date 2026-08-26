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

#include <QList>
#include <QString>

namespace Plugins {

struct OwnedResource {
    QString instanceId;
    QString metadataPath;
    QString resourcePath;
    QString name;
    QString slug;
};

enum class UninstallPolicy {
    // Leave everything alone. Reinstalling the plugin picks the resources back up.
    KeepResources,
    // Delete the metadata, keep the files.
    ForgetMetadata,
    // Delete both.
    DeleteResources,
};

/* Ownership is derived from the .pw.toml files every time it is needed, rather than from a
 * launcher-side index that could drift when instances are copied or edited outside the launcher.
 */
namespace Ownership {

QList<OwnedResource> scan(const QString& pluginId);
QList<OwnedResource> scanRoots(const QString& pluginId, const QStringList& roots);

// Returns how many resources were changed.
int apply(const QList<OwnedResource>& resources, UninstallPolicy policy);

}  // namespace Ownership

}  // namespace Plugins
