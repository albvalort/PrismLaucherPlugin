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

#include "ui/pages/global/PluginsPage.h"
#include "ui_PluginsPage.h"

#include <QFileDialog>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>

#include "Application.h"
#include "DesktopServices.h"
#include "plugins/PluginManager.h"
#include "ui/dialogs/CustomMessageBox.h"

namespace {

constexpr int ID_ROLE = Qt::UserRole;

QString stateText(const Plugins::Plugin* plugin)
{
    switch (plugin->state()) {
        case Plugins::Plugin::State::Enabled:
            return QObject::tr("Enabled");
        case Plugins::Plugin::State::Broken:
            return QObject::tr("Failed to load");
        case Plugins::Plugin::State::Disabled:
            break;
    }
    return QObject::tr("Disabled");
}

// Keeping resources is the default: nothing is destructive unless the user picks otherwise.
std::optional<Plugins::UninstallPolicy> askUninstallPolicy(QWidget* parent, const QString& name, int ownedCount, int instanceCount)
{
    if (ownedCount == 0) {
        const auto answer = CustomMessageBox::selectable(parent, QObject::tr("Remove plugin"),
                                                         QObject::tr("Remove \"%1\"? It has not installed anything.").arg(name),
                                                         QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                                ->exec();
        if (answer != QMessageBox::Yes)
            return {};
        return Plugins::UninstallPolicy::KeepResources;
    }

    QMessageBox box(parent);
    box.setWindowTitle(QObject::tr("Remove plugin"));
    box.setIcon(QMessageBox::Question);
    box.setText(QObject::tr("Remove \"%1\"?").arg(name));
    box.setInformativeText(QObject::tr("It manages %1 resource(s) across %2 instance(s).\n\n"
                                       "Keeping them is safe: the files stay where they are and still record that this plugin "
                                       "installed them, so reinstalling it later restores update checking.")
                               .arg(ownedCount)
                               .arg(instanceCount));

    auto* keep = box.addButton(QObject::tr("Keep resources"), QMessageBox::AcceptRole);
    auto* forget = box.addButton(QObject::tr("Keep files, forget metadata"), QMessageBox::DestructiveRole);
    auto* remove = box.addButton(QObject::tr("Delete resources"), QMessageBox::DestructiveRole);
    box.addButton(QMessageBox::Cancel);
    box.setDefaultButton(keep);

    box.exec();

    if (box.clickedButton() == keep)
        return Plugins::UninstallPolicy::KeepResources;
    if (box.clickedButton() == forget)
        return Plugins::UninstallPolicy::ForgetMetadata;
    if (box.clickedButton() == remove)
        return Plugins::UninstallPolicy::DeleteResources;
    return {};
}

}  // namespace

PluginsPage::PluginsPage(QWidget* parent) : QWidget(parent), m_ui(new Ui::PluginsPage)
{
    m_ui->setupUi(this);

    connect(m_ui->pluginList, &QListWidget::itemSelectionChanged, this, &PluginsPage::onSelectionChanged);
    connect(m_ui->pluginList, &QListWidget::itemChanged, this, &PluginsPage::onItemChanged);

    refresh();
}

PluginsPage::~PluginsPage()
{
    delete m_ui;
}

QIcon PluginsPage::icon() const
{
    auto icon = QIcon::fromTheme("plugins");
    if (icon.isNull())
        icon = QIcon::fromTheme("centralmods");
    return icon;
}

void PluginsPage::retranslate()
{
    m_ui->retranslateUi(this);
    updateDetails();
}

QString PluginsPage::selectedId() const
{
    auto* item = m_ui->pluginList->currentItem();
    return item == nullptr ? QString{} : item->data(ID_ROLE).toString();
}

void PluginsPage::refresh()
{
    const auto previous = selectedId();

    m_populating = true;
    m_ui->pluginList->clear();

    for (auto* plugin : APPLICATION->pluginManager()->plugins()) {
        auto* item = new QListWidgetItem(m_ui->pluginList);
        item->setData(ID_ROLE, plugin->id());
        item->setIcon(plugin->icon());
        item->setText(QString("%1 %2 — %3").arg(plugin->manifest().name, plugin->manifest().version, stateText(plugin)));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(plugin->isEnabled() ? Qt::Checked : Qt::Unchecked);

        if (plugin->id() == previous)
            m_ui->pluginList->setCurrentItem(item);
    }
    m_populating = false;

    onSelectionChanged();
}

void PluginsPage::updateDetails()
{
    auto* plugin = APPLICATION->pluginManager()->plugin(selectedId());
    if (plugin == nullptr) {
        m_ui->detailsLabel->setText(tr("Select a plugin to see its details."));
        return;
    }

    const auto& manifest = plugin->manifest();

    QStringList lines;
    if (!manifest.description.isEmpty())
        lines << manifest.description.toHtmlEscaped();
    if (!manifest.author.isEmpty())
        lines << tr("Author: %1").arg(manifest.author.toHtmlEscaped());
    // The allowlist is the one thing that says what this plugin can reach on the network.
    lines << tr("Allowed hosts: %1").arg(manifest.hosts.join(", ").toHtmlEscaped());
    lines << tr("Folder: %1").arg(plugin->folder().absolutePath().toHtmlEscaped());

    if (plugin->state() == Plugins::Plugin::State::Broken)
        lines << tr("<b>Error:</b> %1").arg(plugin->errorString().toHtmlEscaped());

    m_ui->detailsLabel->setText(lines.join("<br>"));
}

void PluginsPage::onSelectionChanged()
{
    m_ui->removeButton->setEnabled(!selectedId().isEmpty());
    updateDetails();
}

void PluginsPage::onItemChanged(QListWidgetItem* item)
{
    if (m_populating || item == nullptr)
        return;

    const auto id = item->data(ID_ROLE).toString();
    const bool wanted = item->checkState() == Qt::Checked;

    auto* manager = APPLICATION->pluginManager();
    if (!manager->setEnabled(id, wanted) && wanted) {
        auto* plugin = manager->plugin(id);
        CustomMessageBox::selectable(
            this, tr("Could not enable plugin"),
            tr("\"%1\" could not be started:\n\n%2").arg(id, plugin != nullptr ? plugin->errorString() : QString{}), QMessageBox::Warning)
            ->exec();
    }

    refresh();
}

void PluginsPage::on_installButton_clicked()
{
    const auto path = QFileDialog::getOpenFileName(this, tr("Select a plugin archive"), QString(), tr("Plugin archives (*.zip)"));
    if (path.isEmpty())
        return;

    QString error;
    const auto id = APPLICATION->pluginManager()->installFromArchive(path, error);
    if (id.isEmpty()) {
        CustomMessageBox::selectable(this, tr("Could not install plugin"), error, QMessageBox::Warning)->exec();
        return;
    }

    refresh();
    CustomMessageBox::selectable(this, tr("Plugin installed"),
                                 tr("\"%1\" was installed but is not enabled yet. Tick it in the list to start using it.").arg(id),
                                 QMessageBox::Information)
        ->exec();
}

void PluginsPage::on_removeButton_clicked()
{
    const auto id = selectedId();
    auto* manager = APPLICATION->pluginManager();
    auto* plugin = manager->plugin(id);
    if (plugin == nullptr)
        return;

    const auto name = plugin->manifest().name;

    // Scan first, so the numbers the user decides on are real.
    const auto owned = manager->ownedResources(id);
    QSet<QString> instances;
    for (const auto& resource : owned)
        instances.insert(resource.instanceId);

    const auto policy = askUninstallPolicy(this, name, static_cast<int>(owned.size()), static_cast<int>(instances.size()));
    if (!policy.has_value())
        return;

    QString error;
    if (!manager->uninstall(id, policy.value(), error))
        CustomMessageBox::selectable(this, tr("Could not fully remove plugin"), error, QMessageBox::Warning)->exec();

    refresh();
}

void PluginsPage::on_folderButton_clicked()
{
    DesktopServices::openPath(APPLICATION->pluginManager()->pluginsFolder().absolutePath(), true);
}

void PluginsPage::on_reloadButton_clicked()
{
    APPLICATION->pluginManager()->initialize();
    refresh();
}
