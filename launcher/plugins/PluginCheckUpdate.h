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

#include "modplatform/CheckUpdateTask.h"

namespace Plugins {

/* Looks for newer versions of resources installed by plugins.
 *
 * Cannot start from a file hash the way the built-in providers do, since plugins expose no
 * hash-to-project lookup. Works from the project id in the metadata instead.
 */
class PluginCheckUpdate : public CheckUpdateTask {
    Q_OBJECT

   public:
    PluginCheckUpdate(QList<Resource*>& resources,
                      std::vector<Version>& mcVersions,
                      QList<ModPlatform::ModLoaderType> loadersList,
                      ResourceFolderModel* resourceModel);

   public slots:
    bool abort() override;

   protected slots:
    void executeTask() override;

   private:
    void checkNext();
    void onVersions(Resource* resource, const QVector<ModPlatform::IndexedVersion>& versions);

    Task::Ptr m_currentJob;
    QList<Resource*> m_pending;
    int m_index{ 0 };
};

}  // namespace Plugins
