-- SPDX-FileCopyrightText: 2026 albvalort <albvalort@outlook.es>
--
-- SPDX-License-Identifier: GPL-3.0-only

-- A menu-only plugin: it adds entries to the main window and nothing else.
--
-- It implements no search_url or parse_search, so it never appears in the mod download dialog.
--
-- It never builds a widget and cannot draw anything. It returns a list of labels, and when one is
-- chosen it returns a description of what it would like to happen.

-- Every destination must be listed in plugin.json under "hosts", or the launcher refuses to open it.
local LINKS = {
    { id = "wiki",     label = "Minecraft Wiki",      url = "https://minecraft.wiki/" },
    { id = "fabric",   label = "Fabric documentation", url = "https://docs.fabricmc.net/" },
    { id = "neoforge", label = "NeoForge documentation", url = "https://docs.neoforged.net/" },
    { id = "paper",    label = "Paper documentation",  url = "https://docs.papermc.io/" },
}

function on_load()
    prism.log("Quick Links ready with " .. #LINKS .. " destinations")
end

-- Called every time the menu is opened, not once at load.
function menu()
    local items = {}

    for _, link in ipairs(LINKS) do
        items[#items + 1] = { id = link.id, label = link.label }
    end

    items[#items + 1] = { separator = true }
    items[#items + 1] = { id = "about", label = "About Quick Links" }

    return items
end

-- Only two answers exist: show a message, or open a URL. Anything else is ignored.
function on_menu(args)
    for _, link in ipairs(LINKS) do
        if args.id == link.id then
            return { type = "open_url", url = link.url }
        end
    end

    if args.id == "about" then
        return {
            type = "message",
            title = "Quick Links",
            text = "A menu-only plugin: about 40 lines of sandboxed Lua, and no download sources.\n\n"
                .. "It can only do two things - show a message like this one, or ask the launcher to "
                .. "open one of the addresses listed in its manifest. It has no filesystem access, no "
                .. "network access of its own, and no way to touch the interface directly.",
        }
    end
end
