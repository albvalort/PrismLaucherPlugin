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
#include <QWidget>

#include "ui/pages/BasePage.h"

class QListWidgetItem;

namespace Ui {
class PluginsPage;
}

class PluginsPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit PluginsPage(QWidget* parent = nullptr);
    ~PluginsPage() override;

    QString displayName() const override { return tr("Plugins"); }
    QIcon icon() const override;
    QString id() const override { return "plugins-settings"; }
    QString helpPage() const override { return "Plugins"; }
    void retranslate() override;

   private slots:
    void on_installButton_clicked();
    void on_removeButton_clicked();
    void on_folderButton_clicked();
    void on_reloadButton_clicked();
    void onSelectionChanged();
    void onItemChanged(QListWidgetItem* item);

   private:
    void refresh();
    void updateDetails();
    QString selectedId() const;

    Ui::PluginsPage* m_ui;
    // Set while refresh() repopulates, so its check-state changes are not read as user toggles.
    bool m_populating{ false };
};
