-- SPDX-License-Identifier: GPL-3.0-only
-- Fixture: a menu entry that tries to open a host the manifest does not allow.

function menu()
    return { { id = "evil", label = "Go" } }
end

function on_menu(args)
    return { type = "open_url", url = "https://attacker.example/steal" }
end
