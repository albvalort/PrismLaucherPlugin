// SPDX-FileCopyrightText: 2026 albvalort <albvalort@outlook.es>
//
// SPDX-License-Identifier: GPL-3.0-only

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

#include "plugins/LuaRuntime.h"
#include "plugins/Plugin.h"
#include "plugins/PluginManifest.h"
#include "plugins/PluginOwnership.h"

namespace {

QString writeScript(const QTemporaryDir& dir, const QString& body)
{
    const auto path = QDir(dir.path()).absoluteFilePath("script.lua");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return {};
    file.write(body.toUtf8());
    file.close();
    return path;
}

// Fixtures live in testdata because moc mis-lexes some raw string literals badly enough to stop
// seeing the test class at all.
std::unique_ptr<Plugins::Plugin> loadFixture(const QString& entry, const QStringList& hosts = { "api.example.com" });

Plugins::Manifest manifestWithHosts(const QStringList& hosts)
{
    Plugins::Manifest manifest;
    manifest.id = "test";
    manifest.name = "Test";
    manifest.apiVersion = Plugins::CURRENT_API_VERSION;
    manifest.hosts = hosts;
    return manifest;
}

std::unique_ptr<Plugins::Plugin> loadFixture(const QString& entry, const QStringList& hosts)
{
    auto manifest = manifestWithHosts(hosts);
    manifest.entry = entry;
    return std::make_unique<Plugins::Plugin>(manifest, QDir(QFINDTESTDATA("testdata/Plugins")));
}

}  // namespace

class PluginSandboxTest : public QObject {
    Q_OBJECT

   private slots:
    void roundTripsJson()
    {
        QTemporaryDir dir;
        Plugins::LuaRuntime runtime("test");

        const auto path = writeScript(dir, R"(
            function echo(args)
                return { name = args.name, count = args.count + 1, tags = args.tags }
            end
        )");

        QString error;
        QVERIFY2(runtime.loadScript(path, error), qPrintable(error));

        QJsonObject argument{ { "name", "hello" }, { "count", 1 }, { "tags", QJsonArray{ "a", "b" } } };
        auto result = runtime.callForJson("echo", argument, error);

        QVERIFY2(result.has_value(), qPrintable(error));
        const auto object = result->toObject();
        QCOMPARE(object["name"].toString(), QString("hello"));
        QCOMPARE(object["count"].toInt(), 2);
        QCOMPARE(object["tags"].toArray().size(), 2);
    }

    void listsBecomeJsonArrays()
    {
        QTemporaryDir dir;
        Plugins::LuaRuntime runtime("test");

        const auto path = writeScript(dir, R"(
            function build() return { { id = "one" }, { id = "two" } } end
        )");

        QString error;
        QVERIFY2(runtime.loadScript(path, error), qPrintable(error));

        auto result = runtime.callForJson("build", QJsonValue(QJsonValue::Null), error);
        QVERIFY2(result.has_value(), qPrintable(error));
        QVERIFY(result->isArray());
        QCOMPARE(result->toArray().size(), 2);
        QCOMPARE(result->toArray().at(1).toObject()["id"].toString(), QString("two"));
    }

    void deniesFilesystemAndProcessAccess()
    {
        QTemporaryDir dir;
        Plugins::LuaRuntime runtime("test");

        const auto path = writeScript(dir, R"(
            function probe()
                return {
                    os = tostring(os),
                    io = tostring(io),
                    require = tostring(require),
                    load = tostring(load),
                    loadfile = tostring(loadfile),
                    dofile = tostring(dofile),
                }
            end
        )");

        QString error;
        QVERIFY2(runtime.loadScript(path, error), qPrintable(error));

        auto result = runtime.callForJson("probe", QJsonValue(QJsonValue::Null), error);
        QVERIFY2(result.has_value(), qPrintable(error));

        const auto object = result->toObject();
        for (const auto& key : object.keys())
            QCOMPARE(object[key].toString(), QString("nil"));
    }

    // pcall must not let the script carry on after the abort.
    void stopsRunawayScripts()
    {
        QTemporaryDir dir;
        Plugins::LuaLimits limits;
        limits.instructionLimit = 200'000;
        limits.wallClockMs = 1'000;
        Plugins::LuaRuntime runtime("test", limits);

        const auto path = writeScript(dir, R"(
            function spin()
                -- Swallow the abort and keep going, to prove the budget latch holds.
                pcall(function() while true do end end)
                while true do end
            end
        )");

        QString error;
        QVERIFY2(runtime.loadScript(path, error), qPrintable(error));

        auto result = runtime.callForJson("spin", QJsonValue(QJsonValue::Null), error);
        QVERIFY(!result.has_value());
        QVERIFY(!error.isEmpty());
    }

    void refusesBytecode()
    {
        QTemporaryDir dir;
        Plugins::LuaRuntime runtime("test");

        const auto path = QDir(dir.path()).absoluteFilePath("script.lua");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        // The Lua bytecode signature.
        file.write("\x1bLua", 4);
        file.close();

        QString error;
        QVERIFY(!runtime.loadScript(path, error));
    }

    void enforcesHostAllowlist()
    {
        QTemporaryDir dir;
        Plugins::Plugin plugin(manifestWithHosts({ "api.example.com" }), QDir(dir.path()));

        QVERIFY(plugin.isUrlAllowed(QUrl("https://api.example.com/v2/search")));

        QVERIFY(!plugin.isUrlAllowed(QUrl("http://api.example.com/v2/search")));
        QVERIFY(!plugin.isUrlAllowed(QUrl("https://api.example.com.attacker.net/v2")));
        QVERIFY(!plugin.isUrlAllowed(QUrl("https://cdn.example.com/file.jar")));
        QVERIFY(!plugin.isUrlAllowed(QUrl("file:///etc/passwd")));
    }

    void findsResourcesOwnedByAPlugin()
    {
        QTemporaryDir instances;
        QVERIFY(instances.isValid());

        QDir indexDir(instances.path() + "/TestInstance/minecraft/mods/.index");
        QVERIFY(indexDir.mkpath("."));

        QFile metadata(indexDir.absoluteFilePath("example-mod.pw.toml"));
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(R"(name = 'Example Mod'
filename = 'example-mod-1.0.0.jar'
side = 'both'

[download]
mode = 'url'
url = 'https://cdn.example.com/example-mod-1.0.0.jar'
hash-format = 'sha512'
hash = '0123456789abcdef'

[update]
[update.plugin]
plugin-id = 'my-platform'
project-id = 'project-abc'
version-id = 'version-xyz'
)");
        metadata.close();

        const auto owned = Plugins::Ownership::scanRoots("my-platform", { instances.path() });
        QCOMPARE(owned.size(), 1);
        QCOMPARE(owned.first().instanceId, QString("TestInstance"));
        QCOMPARE(owned.first().name, QString("Example Mod"));
        QVERIFY(owned.first().resourcePath.endsWith("mods/example-mod-1.0.0.jar"));

        QVERIFY(Plugins::Ownership::scanRoots("other-platform", { instances.path() }).isEmpty());
    }

    void keepingResourcesTouchesNothing()
    {
        QTemporaryDir instances;
        QVERIFY(instances.isValid());

        QDir indexDir(instances.path() + "/TestInstance/minecraft/mods/.index");
        QVERIFY(indexDir.mkpath("."));

        QFile metadata(indexDir.absoluteFilePath("example-mod.pw.toml"));
        QVERIFY(metadata.open(QIODevice::WriteOnly));
        metadata.write(R"(name = 'Example Mod'
filename = 'example-mod-1.0.0.jar'

[download]
mode = 'url'
url = 'https://cdn.example.com/example-mod-1.0.0.jar'
hash-format = 'sha512'
hash = '0123456789abcdef'

[update]
[update.plugin]
plugin-id = 'my-platform'
project-id = 'project-abc'
version-id = 'version-xyz'
)");
        metadata.close();

        const auto owned = Plugins::Ownership::scanRoots("my-platform", { instances.path() });
        QCOMPARE(owned.size(), 1);

        QCOMPARE(Plugins::Ownership::apply(owned, Plugins::UninstallPolicy::KeepResources), 0);
        QVERIFY(QFileInfo::exists(owned.first().metadataPath));

        QCOMPARE(Plugins::Ownership::apply(owned, Plugins::UninstallPolicy::ForgetMetadata), 1);
        QVERIFY(!QFileInfo::exists(owned.first().metadataPath));
    }

    void menuOnlyPluginLoads()
    {
        auto plugin = loadFixture("menu-only.lua");
        QVERIFY2(plugin->enable(), qPrintable(plugin->errorString()));

        QVERIFY(plugin->providesMenu());
        QVERIFY(!plugin->providesResources());

        const auto items = plugin->menuItems();
        QCOMPARE(items.size(), 3);  // the entry without an id is dropped
        QCOMPARE(items.at(0).id, QString("hello"));
        QVERIFY(items.at(1).separator);

        const auto message = plugin->activateMenuItem("hello");
        QCOMPARE(message.type, Plugins::MenuAction::Type::Message);
        QCOMPARE(message.text, QString("hello"));

        const auto open = plugin->activateMenuItem("site");
        QCOMPARE(open.type, Plugins::MenuAction::Type::OpenUrl);
    }

    void menuCannotOpenArbitraryUrls()
    {
        auto plugin = loadFixture("menu-bad-url.lua");
        QVERIFY2(plugin->enable(), qPrintable(plugin->errorString()));

        QCOMPARE(plugin->activateMenuItem("evil").type, Plugins::MenuAction::Type::None);
    }

    void refusesPluginThatProvidesNothing()
    {
        auto plugin = loadFixture("provides-nothing.lua");

        QVERIFY(!plugin->enable());
        QVERIFY(!plugin->errorString().isEmpty());
    }

    void refusesIncompleteProvider()
    {
        auto plugin = loadFixture("incomplete-provider.lua");

        QVERIFY(!plugin->enable());
        QVERIFY(plugin->errorString().contains("parse_search"));
    }

    void rejectsUnsafeManifests()
    {
        QVERIFY(Plugins::isValidId("my-platform"));
        QVERIFY(!Plugins::isValidId("../escape"));
        QVERIFY(!Plugins::isValidId("Has Capitals"));
        QVERIFY(!Plugins::isValidId(""));
    }
};

QTEST_GUILESS_MAIN(PluginSandboxTest)

#include "PluginSandbox_test.moc"
