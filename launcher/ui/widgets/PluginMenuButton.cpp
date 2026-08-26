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

#include "ui/widgets/PluginMenuButton.h"

#include <QDesktopServices>
#include <QMenu>
#include <QMessageBox>

#include "Application.h"
#include "plugins/PluginManager.h"
#include "ui/dialogs/CustomMessageBox.h"

PluginMenuButton::PluginMenuButton(QWidget* parent) : QToolButton(parent), m_menu(new QMenu(this))
{
    setPopupMode(QToolButton::InstantPopup);
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setMenu(m_menu);
    setToolTip(tr("Plugins"));

    auto icon = QIcon::fromTheme("plugins");
    if (icon.isNull())
        icon = QIcon::fromTheme("centralmods");
    setIcon(icon);

    connect(m_menu, &QMenu::aboutToShow, this, &PluginMenuButton::rebuildMenu);
    connect(APPLICATION->pluginManager(), &Plugins::PluginManager::pluginsChanged, this, &PluginMenuButton::refresh);

    refresh();
}

void PluginMenuButton::refresh()
{
    bool anyMenus = false;
    for (auto* plugin : APPLICATION->pluginManager()->plugins()) {
        if (plugin->providesMenu()) {
            anyMenus = true;
            break;
        }
    }

    setVisible(anyMenus);
}

void PluginMenuButton::rebuildMenu()
{
    m_menu->clear();

    const auto plugins = APPLICATION->pluginManager()->plugins();
    QList<Plugins::Plugin*> contributors;
    for (auto* plugin : plugins) {
        if (plugin->providesMenu())
            contributors.append(plugin);
    }

    for (auto* plugin : contributors) {
        const auto items = plugin->menuItems();
        if (items.isEmpty())
            continue;

        // With several plugins each gets a submenu, so entries are always attributable.
        QMenu* target = m_menu;
        if (contributors.size() > 1) {
            target = m_menu->addMenu(plugin->icon(), plugin->manifest().name);
        }

        for (const auto& item : items) {
            if (item.separator) {
                target->addSeparator();
                continue;
            }

            const auto pluginId = plugin->id();
            const auto itemId = item.id;
            // Third-party labels must not steal mnemonics from the rest of the menu.
            auto* action = target->addAction(QString(item.label).replace('&', "&&"));
            connect(action, &QAction::triggered, this, [this, pluginId, itemId] { runAction(pluginId, itemId); });
        }
    }

    if (m_menu->isEmpty())
        m_menu->addAction(tr("(no plugin menus)"))->setEnabled(false);
}

void PluginMenuButton::runAction(const QString& pluginId, const QString& itemId)
{
    auto* plugin = APPLICATION->pluginManager()->plugin(pluginId);
    if (plugin == nullptr)
        return;

    const auto action = plugin->activateMenuItem(itemId);

    switch (action.type) {
        case Plugins::MenuAction::Type::Message:
            // Plain text, and titled with the plugin's own name.
            CustomMessageBox::selectable(this, tr("%1").arg(action.title), action.text, QMessageBox::Information)->exec();
            break;

        case Plugins::MenuAction::Type::OpenUrl:
            QDesktopServices::openUrl(action.url);
            break;

        case Plugins::MenuAction::Type::None:
            break;
    }
}
