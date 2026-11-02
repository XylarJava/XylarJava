#include "InstanceRepair.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/Component.h"
#include "minecraft/LaunchProfile.h"
#include "minecraft/Library.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "net/HttpMetaCache.h"
#include "settings/SettingsObject.h"

namespace {

/// Deletes a cached download and marks it stale, so it's fetched (and checksummed) again.
bool dropCacheEntry(const QString& base, const QString& path)
{
    auto metacache = APPLICATION->metacache();
    auto entry = metacache->resolveEntry(base, path);
    if (!entry)
        return false;
    const QString file = entry->getFullPath();
    bool removed = false;
    if (QFileInfo(file).isFile())
        removed = QFile::remove(file);
    metacache->evictEntry(entry);
    return removed;
}

}  // namespace

namespace InstanceRepair {

bool looksLikeGraphicsFailure(const QString& gameLog)
{
    static const QStringList signs = { QStringLiteral("Could not choose GLX13 config"),
                                       QStringLiteral("Couldn't set pixel format"),
                                       QStringLiteral("Pixel format not accelerated"),
                                       QStringLiteral("failed to load driver: zink"),
                                       QStringLiteral("glx: failed to create"),
                                       QStringLiteral("GLXBadFBConfig"),
                                       QStringLiteral("WGL: The driver does not appear to support OpenGL"),
                                       QStringLiteral("GLFW error 65542"),
                                       QStringLiteral("GLFW error 65543"),
                                       QStringLiteral("No OpenGL context found in the current thread") };
    for (const QString& sign : signs) {
        if (gameLog.contains(sign, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QStringList repair(BaseInstance* instance, const QString& gameLog)
{
    QStringList done;
    if (!instance)
        return done;

    auto minecraft = dynamic_cast<MinecraftInstance*>(instance);
    if (minecraft) {
        auto packProfile = minecraft->getPackProfile();

        // 1. Version metadata of every component (Minecraft, loaders, ...)
        int metaFiles = 0;
        if (packProfile) {
            for (int i = 0; i < packProfile->rowCount(); i++) {
                auto component = packProfile->getComponent(static_cast<size_t>(i));
                if (!component)
                    continue;
                const QString uid = component->getID();
                const QString version = component->getVersion();
                if (uid.isEmpty() || version.isEmpty())
                    continue;
                if (dropCacheEntry(QStringLiteral("meta"), uid + '/' + version + QStringLiteral(".json")))
                    metaFiles++;
            }
        }
        done << QObject::tr("Refreshed version info (%1 files)").arg(metaFiles);

        // 2. Libraries and the game jar (local libraries are the user's own files: never touched)
        int libraries = 0;
        auto launchProfile = packProfile ? packProfile->getProfile() : nullptr;
        if (launchProfile) {
            const RuntimeContext context = minecraft->runtimeContext();
            QList<LibraryPtr> all;
            all << launchProfile->getLibraries() << launchProfile->getNativeLibraries() << launchProfile->getMavenFiles();
            if (auto mainJar = launchProfile->getMainJar())
                all << mainJar;
            const QDir base(APPLICATION->metacache()->getBasePath(QStringLiteral("libraries")));
            for (const LibraryPtr& library : all) {
                if (!library || library->isLocal() || !library->isActive(context))
                    continue;
                QStringList jar, native, native32, native64;
                library->getApplicableFiles(context, jar, native, native32, native64, QString());
                for (const QString& file : jar + native + native32 + native64) {
                    const QString relative = base.relativeFilePath(file);
                    if (relative.startsWith(QStringLiteral("..")))
                        continue;  // not in the shared libraries folder
                    if (dropCacheEntry(QStringLiteral("libraries"), relative))
                        libraries++;
                }
            }
        }
        done << QObject::tr("Removed %1 library files to download again").arg(libraries);

        // 3. Natives are unpacked on every launch; a half-written folder breaks the game.
        const QString natives = minecraft->getNativePath();
        if (QDir(natives).exists() && FS::deletePath(natives))
            done << QObject::tr("Cleared the natives folder");
    }

    // 4. Java that no longer exists
    auto settings = instance->settings();
    if (settings->get("OverrideJavaLocation").toBool()) {
        const QString javaPath = settings->get("JavaPath").toString();
        if (FS::ResolveExecutable(javaPath).isEmpty()) {
            settings->set("OverrideJavaLocation", false);
            done << QObject::tr("This instance pointed to a Java that's gone, it now uses the global one");
        }
    }
    auto global = APPLICATION->settings();
    if (FS::ResolveExecutable(global->get("JavaPath").toString()).isEmpty()) {
        global->set("AutomaticJavaSwitch", true);
        global->set("AutomaticJavaDownload", true);
        done << QObject::tr("Turned on automatic Java, so the right Java is picked or downloaded");
    }

    // 5. Graphics tweaks that stop OpenGL from starting. They're undone where they were set: on
    //    the instance if it overrides them, otherwise in the global settings.
    if (looksLikeGraphicsFailure(gameLog)) {
        auto turnOff = [&](const QString& key, const QString& overrideKey) {
            if (!settings->get(key).toBool())
                return false;
            if (settings->get(overrideKey).toBool())
                settings->set(key, false);
            else
                global->set(key, false);
            return true;
        };
        if (turnOff(QStringLiteral("UseZink"), QStringLiteral("OverridePerformance")))
            done << QObject::tr("Turned off Zink (OpenGL over Vulkan): the game uses your normal graphics driver again");
        if (turnOff(QStringLiteral("UseDiscreteGpu"), QStringLiteral("OverridePerformance")))
            done << QObject::tr("Stopped forcing the discrete GPU");
        if (gameLog.contains(QStringLiteral("GLFW"), Qt::CaseInsensitive) &&
            turnOff(QStringLiteral("UseNativeGLFW"), QStringLiteral("OverrideNativeWorkarounds")))
            done << QObject::tr("Went back to the GLFW that comes with the game");
    }

    APPLICATION->metacache()->SaveNow();
    for (const QString& line : done)
        qDebug() << "[Repair]" << instance->name() << "-" << line;
    return done;
}

}  // namespace InstanceRepair
