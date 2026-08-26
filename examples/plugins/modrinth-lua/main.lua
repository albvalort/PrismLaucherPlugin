-- SPDX-FileCopyrightText: 2026 albvalort <albvalort@outlook.es>
--
-- SPDX-License-Identifier: GPL-3.0-only

-- Reference plugin for the Prism Launcher plugin API.
--
-- Talks to Modrinth, which the launcher already supports natively, so its results can be compared
-- side by side with the built-in page.
--
-- The plugin performs no requests: it returns URLs, and the launcher fetches them, only from the
-- hosts listed in plugin.json. Responses arrive already decoded. There is no io, no os and no
-- require; prism.log and prism.url_encode are the whole host surface.

local API = "https://api.modrinth.com/v2"

-- Modrinth calls these "project types".
local PROJECT_TYPES = {
    mod = "mod",
    resourcepack = "resourcepack",
    shaderpack = "shader",
    datapack = "mod",
    texturepack = "resourcepack",
}

function on_load()
    prism.log("Modrinth example plugin loaded")
end

local function facet(key, values)
    if not values or #values == 0 then
        return nil
    end
    local parts = {}
    for _, value in ipairs(values) do
        parts[#parts + 1] = string.format('"%s:%s"', key, value)
    end
    return "[" .. table.concat(parts, ",") .. "]"
end

function search_url(args)
    local facets = {}

    local projectType = PROJECT_TYPES[args.resource_type] or "mod"
    facets[#facets + 1] = facet("project_type", { projectType })

    -- Data packs are ordinary projects on Modrinth, distinguished by a category.
    if args.resource_type == "datapack" then
        facets[#facets + 1] = facet("categories", { "datapack" })
    else
        facets[#facets + 1] = facet("categories", args.loaders)
    end

    facets[#facets + 1] = facet("versions", args.game_versions)
    facets[#facets + 1] = facet("categories", args.categories)

    local groups = {}
    for _, group in ipairs(facets) do
        groups[#groups + 1] = group
    end

    local url = API
        .. "/search?limit=25&offset=" .. tostring(args.offset or 0)
        .. "&index=" .. prism.url_encode(args.sort or "relevance")
        .. "&query=" .. prism.url_encode(args.query or "")

    if #groups > 0 then
        url = url .. "&facets=" .. prism.url_encode("[" .. table.concat(groups, ",") .. "]")
    end

    return url
end

local function normalize_project(hit)
    return {
        id = hit.project_id or hit.id,
        name = hit.title,
        slug = hit.slug,
        description = hit.description,
        author = hit.author,
        icon_url = hit.icon_url,
        website_url = hit.slug and ("https://modrinth.com/mod/" .. hit.slug) or nil,
    }
end

function parse_search(response)
    local results = {}
    for _, hit in ipairs(response.hits or {}) do
        results[#results + 1] = normalize_project(hit)
    end
    return results
end

function project_url(args)
    return API .. "/project/" .. prism.url_encode(args.project_id)
end

function parse_project(response)
    local project = normalize_project(response)
    project.body = response.body
    project.source_url = response.source_url
    project.issues_url = response.issues_url
    project.wiki_url = response.wiki_url
    project.discord_url = response.discord_url
    project.status = response.status
    return project
end

function versions_url(args)
    local url = API .. "/project/" .. prism.url_encode(args.project_id) .. "/version"

    -- Modrinth expects these as JSON arrays in the query string.
    local function json_array(values)
        local parts = {}
        for _, value in ipairs(values or {}) do
            parts[#parts + 1] = string.format('"%s"', value)
        end
        return "[" .. table.concat(parts, ",") .. "]"
    end

    local query = {}
    if args.game_versions and #args.game_versions > 0 then
        query[#query + 1] = "game_versions=" .. prism.url_encode(json_array(args.game_versions))
    end
    if args.loaders and #args.loaders > 0 then
        query[#query + 1] = "loaders=" .. prism.url_encode(json_array(args.loaders))
    end

    if #query > 0 then
        url = url .. "?" .. table.concat(query, "&")
    end
    return url
end

local function primary_file(files)
    for _, file in ipairs(files or {}) do
        if file.primary then
            return file
        end
    end
    return (files or {})[1]
end

function parse_versions(response)
    local versions = {}

    for _, version in ipairs(response) do
        local file = primary_file(version.files)

        -- The launcher drops versions without a hash anyway.
        if file and file.hashes and (file.hashes.sha512 or file.hashes.sha1) then
            local hash_format = file.hashes.sha512 and "sha512" or "sha1"

            versions[#versions + 1] = {
                id = version.id,
                project_id = version.project_id,
                name = version.name,
                version_number = version.version_number,
                release_type = version.version_type,
                download_url = file.url,
                filename = file.filename,
                hash = file.hashes[hash_format],
                hash_format = hash_format,
                game_versions = version.game_versions,
                loaders = version.loaders,
                date = version.date_published,
                changelog = version.changelog,
            }
        end
    end

    return versions
end

function sortings()
    return {
        { id = "relevance", name = "Sort by Relevance" },
        { id = "downloads", name = "Sort by Downloads" },
        { id = "follows",   name = "Sort by Follows" },
        { id = "newest",    name = "Sort by Newest" },
        { id = "updated",   name = "Sort by Last Updated" },
    }
end

function on_unload()
    prism.log("Modrinth example plugin unloaded")
end
