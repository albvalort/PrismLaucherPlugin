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

#include <QToolButton>

class QMenu;

/* Toolbar button holding the menus contributed by enabled plugins.
 *
 * Plugins never build widgets: they return a list of labels, and this turns that into a QMenu.
 * Hides itself when no enabled plugin contributes a menu.
 */
class PluginMenuButton : public QToolButton {
    Q_OBJECT

   public:
    explicit PluginMenuButton(QWidget* parent = nullptr);

   private slots:
    void refresh();

    // On open, so a plugin can reflect its own state.
    void rebuildMenu();

   private:
    void runAction(const QString& pluginId, const QString& itemId);

    QMenu* m_menu;
};
