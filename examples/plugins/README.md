<!--
SPDX-FileCopyrightText: 2026 albvalort <albvalort@outlook.es>

SPDX-License-Identifier: GPL-3.0-only
-->

# Plugins

A plugin is a folder under `plugins/` in the launcher's data directory, containing a `plugin.json`
manifest and a Lua script. Plugins can add resource providers, menu entries in the main window, or
both.

## Design

`ResourceAPI` asks providers for URLs and for help reshaping responses; the launcher does the
networking. Plugins keep that shape:

- A plugin returns a URL string. The launcher checks it against the `hosts` list in the plugin's
  manifest, refuses anything that is not `https`, and fetches it itself.
- There is no `io`, no `os`, no `require`, no `load`.
- Downloads are hash-verified. A version without a hash is discarded.

## Manifest

```json
{
  "id": "my-platform",
  "name": "My Platform",
  "version": "1.0.0",
  "author": "you",
  "description": "Adds My Platform as a mod source.",
  "entry": "main.lua",
  "api_version": 1,
  "hosts": ["api.my-platform.example", "cdn.my-platform.example"],
  "resource_types": ["mod", "resourcepack"]
}
```

`id` must be 2-64 characters of `a-z`, `0-9` or `-`, and must match the folder name. `hosts` is
required and non-empty; every URL the plugin returns is matched against it by exact hostname.
`resource_types` accepts `mod`, `resourcepack`, `texturepack`, `shaderpack` and `datapack`, and
defaults to mods only.

## Lua API

A plugin must provide either all four provider functions or `menu`. Anything less is refused at load
time.

```lua
function search_url(args)         --> string
function parse_search(response)   --> list of projects
function versions_url(args)       --> string
function parse_versions(response) --> list of versions
```

Optional:

```lua
function project_url(args)        --> string     -- enables the details pane
function parse_project(response)  --> project
function sortings()               --> list of { id =, name = }
function menu()                   --> list of { id =, label = } or { separator = true }
function on_menu(args)            --> { type = "message", title =, text = }
                                  --  or { type = "open_url", url = }
function on_load()
function on_unload()
function on_uninstall()
```

`search_url(args)` receives `query`, `offset`, `resource_type`, `loaders`, `game_versions`,
`categories`, `open_source`, and optionally `sort` and `side`. `versions_url(args)` receives
`project_id`, `resource_type`, `loaders` and `game_versions`.

A project:

```lua
{ id =, name =, slug =, description =, author =, author_url =, icon_url =, website_url =, side =,
  -- parse_project only:
  body =, source_url =, issues_url =, wiki_url =, discord_url =, status = }
```

A version. `hash` and `hash_format` are required; versions without them are dropped. `hash_format`
may be `sha512`, `sha256`, `sha1` or `md5`:

```lua
{ id =, project_id =, name =, version_number =, release_type =, download_url =, filename =,
  hash =, hash_format =, game_versions = { }, loaders = { }, date =, changelog =, side = }
```

`menu` entries are re-read every time the menu opens. `on_menu` URLs go through the same host
allowlist as everything else.

### Host functions

```lua
prism.log(message)
prism.url_encode(text)
```

That is the whole surface.

## Limits

| Limit | Default |
| --- | --- |
| Memory | 32 MiB |
| Instructions per call | 20,000,000 |
| Wall clock per call | 2,000 ms |

Exceeding any of them aborts the call, and `pcall` cannot resume it: once a budget is blown the
check keeps failing for the rest of the call.

## Uninstalling

A plugin that installs a resource writes ordinary packwiz metadata with a generic ownership record:

```toml
[update.plugin]
plugin-id = "my-platform"
project-id = "abc123"
version-id = "xyz789"
```

That record is the only source of truth for ownership; there is no launcher-side index to drift when
instances are copied or edited while the launcher is closed. So:

- Disabling a plugin touches nothing. Re-enabling restores update checking.
- A missing plugin does not break anything. `[update.plugin]` is parsed whether or not the plugin is
  installed, so the resource stays recognised; only update checking is skipped, and the update
  dialog says why.
- Uninstalling scans the instances, shows what the plugin owns, and offers: keep the resources
  (default, touches nothing), keep the files but drop the metadata, or delete both.

`on_uninstall()` is called first, but nothing above depends on it: a plugin folder can also be
deleted while the launcher is not running.

## Examples

- `modrinth-lua/` - a provider. Reimplements Modrinth search, so its page can be compared against
  the built-in one.
- `quick-links/` - a menu, with no provider at all.

```sh
cd examples/plugins/quick-links && zip -r ../quick-links.zip .
```

Then **Settings -> Plugins -> Install from file...**. Plugins install disabled.
