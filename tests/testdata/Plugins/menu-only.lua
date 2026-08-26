-- SPDX-License-Identifier: GPL-3.0-only
-- Fixture: a plugin that only contributes a menu, with no provider functions at all.

function menu()
    return {
        { id = "hello", label = "Say hello" },
        { separator = true },
        { id = "site", label = "Open site" },
        -- No id: the host drops this one rather than showing an entry it could never act on.
        { label = "no id, dropped" },
    }
end

function on_menu(args)
    if args.id == "hello" then
        return { type = "message", title = "Hi", text = "hello" }
    end
    if args.id == "site" then
        return { type = "open_url", url = "https://api.example.com/" }
    end
end
