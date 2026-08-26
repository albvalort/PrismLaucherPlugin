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

#include "plugins/Plugin.h"

#include <algorithm>

#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>

namespace Plugins {

namespace {

const char* const PROVIDER_FUNCTIONS[] = { "search_url", "parse_search", "versions_url", "parse_versions" };

constexpr int MAX_MENU_ITEMS = 32;
constexpr int MAX_MESSAGE_LENGTH = 4000;

}  // namespace

Plugin::Plugin(Manifest manifest, QDir folder) : m_manifest(std::move(manifest)), m_folder(std::move(folder)) {}

Plugin::~Plugin()
{
    disable();
}

bool Plugin::enable()
{
    if (m_state == State::Enabled)
        return true;

    m_error.clear();
    m_runtime = std::make_unique<LuaRuntime>(m_manifest.id);

    const auto entry = m_folder.absoluteFilePath(m_manifest.entry);
    if (!QFileInfo::exists(entry)) {
        m_error = QObject::tr("The entry point \"%1\" does not exist.").arg(m_manifest.entry);
        m_runtime.reset();
        m_state = State::Broken;
        return false;
    }

    if (!m_runtime->loadScript(entry, m_error)) {
        m_runtime.reset();
        m_state = State::Broken;
        return false;
    }

    // A plugin may be a provider, a menu contribution, or both, but it has to be one of them.
    const bool hasAnyProviderFunction = std::any_of(std::begin(PROVIDER_FUNCTIONS), std::end(PROVIDER_FUNCTIONS),
                                                    [this](const char* name) { return m_runtime->hasFunction(name); });

    if (hasAnyProviderFunction) {
        // Check the whole contract up front, rather than failing mid-download.
        for (const char* function : PROVIDER_FUNCTIONS) {
            if (!m_runtime->hasFunction(function)) {
                m_error = QObject::tr("The plugin defines some provider functions but is missing \"%1\".").arg(QString::fromUtf8(function));
                m_runtime.reset();
                m_state = State::Broken;
                return false;
            }
        }
    } else if (!m_runtime->hasFunction("menu")) {
        m_error = QObject::tr("The plugin does not provide anything: it defines neither the resource provider functions nor \"menu\".");
        m_runtime.reset();
        m_state = State::Broken;
        return false;
    }

    if (!m_runtime->callOptionalHook("on_load", m_error)) {
        m_runtime.reset();
        m_state = State::Broken;
        return false;
    }

    m_state = State::Enabled;
    qDebug() << "[Plugin] Enabled" << m_manifest.id << m_manifest.version;
    return true;
}

void Plugin::disable()
{
    if (m_runtime) {
        QString error;
        if (!m_runtime->callOptionalHook("on_unload", error))
            qWarning() << "[Plugin]" << m_manifest.id << "failed during on_unload:" << error;
        m_runtime.reset();
    }

    // Broken plugins stay Broken so the settings page can keep showing why.
    if (m_state == State::Enabled)
        m_state = State::Disabled;
}

void Plugin::notifyUninstall()
{
    // A disabled plugin should still get its chance to clear whatever it cached.
    const bool wasEnabled = isEnabled();
    if (!wasEnabled && !enable()) {
        qWarning() << "[Plugin]" << m_manifest.id << "could not be started for on_uninstall:" << m_error;
        return;
    }

    QString error;
    if (!m_runtime->callOptionalHook("on_uninstall", error))
        qWarning() << "[Plugin]" << m_manifest.id << "failed during on_uninstall:" << error;

    disable();
}

bool Plugin::providesResources() const
{
    if (!isEnabled())
        return false;

    return std::all_of(std::begin(PROVIDER_FUNCTIONS), std::end(PROVIDER_FUNCTIONS),
                       [this](const char* name) { return m_runtime->hasFunction(name); });
}

bool Plugin::providesMenu() const
{
    return isEnabled() && m_runtime->hasFunction("menu");
}

QList<MenuItem> Plugin::menuItems()
{
    QList<MenuItem> items;
    if (!providesMenu())
        return items;

    QString error;
    const auto result = m_runtime->callForJson("menu", QJsonValue(QJsonValue::Null), error);
    if (!result.has_value()) {
        qWarning() << "[Plugin]" << m_manifest.id << ": menu failed:" << error;
        return items;
    }
    if (!result->isArray()) {
        qWarning() << "[Plugin]" << m_manifest.id << ": menu did not return a list.";
        return items;
    }

    for (const auto& entry : result->toArray()) {
        if (items.size() >= MAX_MENU_ITEMS) {
            qWarning() << "[Plugin]" << m_manifest.id << "returned more than" << MAX_MENU_ITEMS << "menu items; ignoring the rest.";
            break;
        }

        const auto object = entry.toObject();
        MenuItem item;
        item.separator = object["separator"].toBool();

        if (item.separator) {
            items.append(item);
            continue;
        }

        item.id = object["id"].toString();
        item.label = object["label"].toString();
        if (item.id.isEmpty() || item.label.isEmpty())
            continue;

        items.append(item);
    }

    return items;
}

MenuAction Plugin::activateMenuItem(const QString& itemId)
{
    MenuAction action;
    if (!isEnabled() || !m_runtime->hasFunction("on_menu"))
        return action;

    QString error;
    const auto result = m_runtime->callForJson("on_menu", QJsonObject{ { "id", itemId } }, error);
    if (!result.has_value()) {
        qWarning() << "[Plugin]" << m_manifest.id << ": on_menu failed:" << error;
        return action;
    }
    if (!result->isObject())
        return action;

    const auto object = result->toObject();
    const auto type = object["type"].toString();

    if (type == QStringLiteral("message")) {
        action.type = MenuAction::Type::Message;
        action.title = object["title"].toString(m_manifest.name);
        action.text = object["text"].toString();
        if (action.text.size() > MAX_MESSAGE_LENGTH)
            action.text.truncate(MAX_MESSAGE_LENGTH);
        if (action.text.isEmpty())
            action.type = MenuAction::Type::None;
        return action;
    }

    if (type == QStringLiteral("open_url")) {
        const QUrl url(object["url"].toString());
        // Opening a URL is not a smaller capability than fetching one.
        if (!isUrlAllowed(url)) {
            qWarning() << "[Plugin]" << m_manifest.id << "asked to open a URL outside its declared hosts:" << object["url"].toString();
            return action;
        }
        action.type = MenuAction::Type::OpenUrl;
        action.url = url;
        return action;
    }

    return action;
}

bool Plugin::isUrlAllowed(const QUrl& url) const
{
    if (!url.isValid())
        return false;

    // Plain HTTP would let an attacker rewrite search results into download URLs of their choosing.
    if (url.scheme() != QStringLiteral("https"))
        return false;

    const auto host = url.host().toLower();
    if (host.isEmpty())
        return false;

    // Exact matches only: a suffix match on "example.com" would allow "example.com.attacker.net".
    return m_manifest.hosts.contains(host);
}

QIcon Plugin::icon() const
{
    const auto path = m_folder.absoluteFilePath(QStringLiteral("icon.png"));
    if (QFileInfo::exists(path))
        return QIcon(path);
    auto fallback = QIcon::fromTheme(QStringLiteral("plugins"));
    return fallback.isNull() ? QIcon::fromTheme(QStringLiteral("centralmods")) : fallback;
}

}  // namespace Plugins
