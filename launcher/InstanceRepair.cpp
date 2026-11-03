#include "InstanceRepair.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <memory>

#include "Application.h"
#include "BaseInstance.h"
#include "BuildConfig.h"
#include "FileSystem.h"
#include "HardwareInfo.h"
#include "minecraft/Component.h"
#include "minecraft/LaunchProfile.h"
#include "minecraft/Library.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/Mod.h"
#include "minecraft/mod/tasks/LocalModParseTask.h"
#include "net/HttpMetaCache.h"
#include "servers/LocalServer.h"
#include "settings/SettingsObject.h"

namespace {

constexpr int kMaxSuspects = 2;  ///< mods disabled at most for one crash, so a wrong guess doesn't empty the folder
constexpr int kTimeoutMs = 30000;

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

/// Ids that are the game, Java or a mod loader rather than a mod in the folder.
bool isPlatform(const QString& id)
{
    static const QSet<QString> ids = { QStringLiteral("minecraft"),    QStringLiteral("java"),     QStringLiteral("fabricloader"),
                                       QStringLiteral("fabric-loader"), QStringLiteral("quilt_loader"), QStringLiteral("forge"),
                                       QStringLiteral("neoforge"),     QStringLiteral("fml"),      QStringLiteral("javafml"),
                                       QStringLiteral("mixinextras"),  QStringLiteral("mixin") };
    return ids.contains(id.toLower());
}

/// APIs that many mods need: a crash naming them is almost never their fault.
bool isSharedApi(const QString& id)
{
    return id == QLatin1String("fabric-api") || id == QLatin1String("fabric") || id == QLatin1String("quilted_fabric_api") ||
           id.startsWith(QLatin1String("fabric-"));
}

/// Modrinth's loader names for this instance; empty for vanilla.
QStringList modrinthLoaders(PackProfile* profile)
{
    if (!profile)
        return {};
    if (!profile->getComponentVersion(QStringLiteral("org.quiltmc.quilt-loader")).isEmpty())
        return { QStringLiteral("quilt"), QStringLiteral("fabric") };
    if (!profile->getComponentVersion(QStringLiteral("net.fabricmc.fabric-loader")).isEmpty())
        return { QStringLiteral("fabric") };
    if (!profile->getComponentVersion(QStringLiteral("net.neoforged")).isEmpty())
        return { QStringLiteral("neoforge") };
    if (!profile->getComponentVersion(QStringLiteral("net.minecraftforge")).isEmpty())
        return { QStringLiteral("forge") };
    return {};
}

struct ModIndex {
    QHash<QString, QStringList> files;  ///< mod id -> enabled files with it
    QHash<QString, QString> names;      ///< mod id -> name
};

/// The enabled mods in @p folder by their id.
ModIndex indexMods(const QString& folder)
{
    ModIndex index;
    if (folder.isEmpty())
        return index;
    const auto entries = QDir(folder).entryInfoList({ QStringLiteral("*.jar"), QStringLiteral("*.zip") }, QDir::Files);
    for (const QFileInfo& file : entries) {
        Mod mod(file);
        ModUtils::process(mod, ModUtils::ProcessingLevel::BasicInfoOnly);
        const QString id = mod.details().mod_id;
        if (id.isEmpty())
            continue;
        index.files[id].append(file.absoluteFilePath());
        index.names.insert(id, mod.details().name.isEmpty() ? id : mod.details().name);
    }
    return index;
}

/// Renames a mod to "<file>.disabled", as the Mods page does.
bool disableFile(const QString& path)
{
    if (!QFileInfo(path).isFile())
        return false;
    QString target = path + QStringLiteral(".disabled");
    for (int i = 2; QFileInfo::exists(target); i++)
        target = path + QStringLiteral(".%1.disabled").arg(i);
    return QFile::rename(path, target);
}

void addOnce(QStringList& list, const QString& value)
{
    if (!value.isEmpty() && !list.contains(value))
        list.append(value);
}

/// Removes every argument mentioning @p option ("-XX:+Foo", "-XX:Foo=1"...) from a JVM argument line.
QString withoutOption(const QString& args, const QString& option)
{
    QString name = option;
    name.remove(QRegularExpression(QStringLiteral("^-*(XX:)?[+-]?")));
    name = name.section(QLatin1Char('='), 0, 0);
    QStringList kept;
    for (const QString& arg : args.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts)) {
        if (!name.isEmpty() && arg.contains(name))
            continue;
        kept << arg;
    }
    return kept.join(QLatin1Char(' '));
}

/// Downloads mods from Modrinth one after the other, with the mods they need.
class ModrinthFetch : public std::enable_shared_from_this<ModrinthFetch> {
   public:
    struct Item {
        QString id;          ///< mod id / Modrinth slug or project id
        QString oldFile;     ///< for an update: the file it replaces
        QStringList neededBy;  ///< for a missing mod: files that need it (disabled when it can't be found)
    };

    QNetworkAccessManager* network = nullptr;
    QString folder;
    QString gameVersion;
    QStringList loaders;
    QSet<QString> installed;  ///< ids present in the folder
    QList<Item> queue;
    QSet<QString> seen;
    QHash<QString, QString> names;
    QStringList done;
    QStringList failed;
    std::function<void()> finished;

    void next()
    {
        if (queue.isEmpty()) {
            if (finished)
                finished();
            return;
        }
        const Item item = queue.takeFirst();
        auto self = shared_from_this();
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("game_versions"), QStringLiteral("[\"%1\"]").arg(gameVersion));
        if (!loaders.isEmpty()) {
            QStringList quoted;
            for (const QString& loader : loaders)
                quoted << QStringLiteral("\"%1\"").arg(loader);
            query.addQueryItem(QStringLiteral("loaders"), QStringLiteral("[%1]").arg(quoted.join(QLatin1Char(','))));
        }
        QUrl url(QStringLiteral("https://api.modrinth.com/v2/project/%1/version").arg(QString::fromLatin1(QUrl::toPercentEncoding(item.id))));
        url.setQuery(query);
        get(url, [self, item](int status, const QByteArray& body) {
            const QJsonArray versions = status == 200 ? QJsonDocument::fromJson(body).array() : QJsonArray();
            QJsonObject pick;
            for (const auto& value : versions) {
                const QJsonObject version = value.toObject();
                if (pick.isEmpty())
                    pick = version;
                if (version.value("version_type").toString() == QLatin1String("release")) {
                    pick = version;
                    break;
                }
            }
            if (pick.isEmpty())
                return self->giveUp(item, status == 404 ? QStringLiteral("not on Modrinth") : QStringLiteral("no build for Minecraft %1").arg(self->gameVersion));
            self->download(item, pick);
        });
    }

   private:
    QString nameOf(const QString& id) const { return names.value(id, id); }

    void get(const QUrl& url, std::function<void(int, const QByteArray&)> done)
    {
        QNetworkRequest request(url);
        request.setTransferTimeout(kTimeoutMs);
        request.setRawHeader("User-Agent", QStringLiteral("%1/%2 (instance repair)").arg(BuildConfig.LAUNCHER_NAME, BuildConfig.versionString()).toUtf8());
        QNetworkReply* reply = network->get(request);
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done] {
            reply->deleteLater();
            done(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), reply->readAll());
        });
    }

    void download(const Item& item, const QJsonObject& version)
    {
        QJsonObject file;
        for (const auto& value : version.value("files").toArray()) {
            if (file.isEmpty() || value.toObject().value("primary").toBool())
                file = value.toObject();
        }
        const QString fileName = QFileInfo(file.value("filename").toString()).fileName();
        const QString sha1 = file.value("hashes").toObject().value("sha1").toString();
        const QUrl url(file.value("url").toString());
        if (fileName.isEmpty() || !url.isValid() || url.scheme() != QLatin1String("https"))
            return giveUp(item, QStringLiteral("no file to download"));
        const QString target = QDir(folder).filePath(fileName);
        if (!item.oldFile.isEmpty() && QFileInfo(item.oldFile).fileName() == fileName)
            return giveUp(item, QStringLiteral("no other build for Minecraft %1").arg(gameVersion));

        auto self = shared_from_this();
        get(url, [self, item, version, target, sha1](int status, const QByteArray& data) {
            if (status != 200 || data.isEmpty())
                return self->giveUp(item, QStringLiteral("download failed"));
            if (!sha1.isEmpty() && QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex() != sha1.toLatin1())
                return self->giveUp(item, QStringLiteral("the download was damaged"));
            QSaveFile out(target);
            if (!out.open(QIODevice::WriteOnly) || out.write(data) != data.size() || !out.commit())
                return self->giveUp(item, QStringLiteral("couldn't write %1").arg(target));

            const QString name = self->nameOf(item.id);
            if (!item.oldFile.isEmpty()) {
                disableFile(item.oldFile);
                self->done << QObject::tr("Updated %1 to %2, the build for Minecraft %3")
                                  .arg(name, version.value("version_number").toString(), self->gameVersion);
            } else {
                self->done << QObject::tr("Installed %1 %2 from Modrinth").arg(name, version.value("version_number").toString());
            }
            self->installed.insert(item.id);
            self->queueDependencies(version);
        });
    }

    /// Mods the new file needs: their project ids are looked up as slugs and added when missing.
    void queueDependencies(const QJsonObject& version)
    {
        QStringList projects;
        for (const auto& value : version.value("dependencies").toArray()) {
            const QJsonObject dependency = value.toObject();
            const QString project = dependency.value("project_id").toString();
            if (dependency.value("dependency_type").toString() == QLatin1String("required") && !project.isEmpty() && !seen.contains(project))
                projects << project;
        }
        if (projects.isEmpty())
            return next();
        auto self = shared_from_this();
        QUrl url(QStringLiteral("https://api.modrinth.com/v2/projects"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("ids"), QStringLiteral("[\"%1\"]").arg(projects.join(QStringLiteral("\",\""))));
        url.setQuery(query);
        get(url, [self, projects](int status, const QByteArray& body) {
            if (status == 200) {
                for (const auto& value : QJsonDocument::fromJson(body).array()) {
                    const QJsonObject project = value.toObject();
                    const QString slug = project.value("slug").toString();
                    const QString id = project.value("id").toString();
                    self->seen.insert(id);
                    QString underscored = slug;
                    underscored.replace(QLatin1Char('-'), QLatin1Char('_'));
                    if (slug.isEmpty() || self->installed.contains(slug) || self->installed.contains(underscored) || self->seen.contains(slug))
                        continue;
                    self->seen.insert(slug);
                    self->names.insert(slug, project.value("title").toString(slug));
                    self->queue.append({ slug, QString(), {} });
                }
            }
            self->next();
        });
    }

    void giveUp(const Item& item, const QString& why)
    {
        const QString name = nameOf(item.id);
        if (!item.oldFile.isEmpty()) {
            if (disableFile(item.oldFile))
                failed << QObject::tr("%1: %2, so it's turned off").arg(name, why);
            else
                failed << QObject::tr("%1: %2").arg(name, why);
        } else if (!item.neededBy.isEmpty()) {
            QStringList off;
            for (const QString& file : item.neededBy) {
                if (disableFile(file))
                    off << QFileInfo(file).fileName();
            }
            failed << (off.isEmpty() ? QObject::tr("Couldn't get %1 (%2)").arg(name, why)
                                     : QObject::tr("Couldn't get %1 (%2), so the mods that need it are turned off: %3")
                                           .arg(name, why, off.join(QStringLiteral(", "))));
        } else {
            failed << QObject::tr("Couldn't get %1 (%2)").arg(name, why);
        }
        next();
    }
};

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

QString lastGameLog(BaseInstance* instance)
{
    if (!instance)
        return {};
    QString text;
    const QDir root(instance->gameRoot());
    QFile latest(root.filePath(QStringLiteral("logs/latest.log")));
    if (latest.open(QIODevice::ReadOnly))
        text = QString::fromUtf8(latest.readAll().right(2 * 1024 * 1024));
    // A crash report newer than the log belongs to the same run.
    const auto reports = QDir(root.filePath(QStringLiteral("crash-reports"))).entryInfoList({ QStringLiteral("crash-*.txt") }, QDir::Files, QDir::Time);
    if (!reports.isEmpty() && (!latest.exists() || reports.first().lastModified() >= QFileInfo(latest).lastModified().addSecs(-5))) {
        QFile report(reports.first().absoluteFilePath());
        if (report.open(QIODevice::ReadOnly))
            text += QLatin1Char('\n') + QString::fromUtf8(report.readAll().left(512 * 1024));
    }
    return text;
}

Diagnosis diagnose(BaseInstance* instance, const QString& gameLog)
{
    Diagnosis d;
    if (!instance)
        return d;
    const QString log = gameLog.trimmed().isEmpty() ? lastGameLog(instance) : gameLog;
    auto minecraft = dynamic_cast<MinecraftInstance*>(instance);
    const QString modsDir = minecraft ? QDir::cleanPath(minecraft->modsRoot()) : QString();
    const ModIndex mods = indexMods(modsDir);
    for (auto it = mods.files.begin(); it != mods.files.end(); ++it)
        d.modFiles.insert(it.key(), it.value().first());
    d.modNames = mods.names;
    auto nameOf = [&](const QString& id, const QString& fallback = QString()) {
        return mods.names.value(id, fallback.isEmpty() ? id : fallback);
    };
    QStringList causes;

    // Mods in the folder twice: the loaders refuse to start. The newest file stays.
    for (auto it = mods.files.begin(); it != mods.files.end(); ++it) {
        if (it.value().size() < 2)
            continue;
        QStringList files = it.value();
        std::sort(files.begin(), files.end(),
                  [](const QString& a, const QString& b) { return QFileInfo(a).lastModified() > QFileInfo(b).lastModified(); });
        for (int i = 1; i < files.size(); i++) {
            addOnce(d.modsToDisable, files.at(i));
            d.plan << QObject::tr("Turn off the older copy of %1 (%2)").arg(nameOf(it.key()), QFileInfo(files.at(i)).fileName());
        }
        addOnce(causes, QObject::tr("Some mods are in the mods folder twice."));
    }

    // Fabric / Quilt: "Mod 'Iris' (iris) 1.7.0 requires any version of fabric-api, which is missing!"
    static const QRegularExpression fabricMissing(
        QStringLiteral(R"(Mod '([^'\n]+)' \(([\w.\-]+)\)[^\n]*? requires [^\n]*? of (?:'[^'\n]*' \()?([\w.\-]+)\)?, which is missing)"));
    static const QRegularExpression fabricInstall(QStringLiteral(R"(^\s*-\s*Install ([\w.\-]+),)"), QRegularExpression::MultilineOption);
    static const QRegularExpression fabricWrong(QStringLiteral(
        R"(Mod '([^'\n]+)' \(([\w.\-]+)\)[^\n]*? requires ([^\n]*?) of (?:'[^'\n]*' \()?([\w.\-]+)\)?, but only the wrong version is present)"));
    static const QRegularExpression fabricReplace(QStringLiteral(R"(Replace '([^'\n]+)' \(([\w.\-]+)\))"));
    static const QRegularExpression fabricRemove(QStringLiteral(R"(Remove mod '([^'\n]+)' \(([\w.\-]+)\))"));
    // Forge / NeoForge: "Mod ID: 'architectury', Requested by: 'rei', Expected range: '[9,)', Actual version: '[MISSING]'"
    static const QRegularExpression forgeDependency(
        QStringLiteral(R"(Mod ID: '([\w.\-]+)', Requested by: '([\w.\-]+)', Expected range: '([^'\n]*)', Actual version: '([^'\n]*)')"));

    auto needs = [&](const QString& missing, const QString& requester) {
        if (missing.isEmpty() || isPlatform(missing) || mods.files.contains(missing))
            return;
        if (!d.modsToInstall.contains(missing))
            d.plan << QObject::tr("Download %1 from Modrinth").arg(missing);
        addOnce(d.modsToInstall, missing);
        if (!requester.isEmpty())
            d.requiredBy.append({ missing, requester });
        addOnce(causes, QObject::tr("A mod needs another mod that isn't installed."));
    };
    auto update = [&](const QString& id) {
        if (id.isEmpty() || isPlatform(id) || !mods.files.contains(id) || d.modsToUpdate.contains(id))
            return;
        d.modsToUpdate << id;
        d.plan << QObject::tr("Swap %1 for its build for this Minecraft version (from Modrinth)").arg(nameOf(id));
        addOnce(causes, QObject::tr("A mod is made for another Minecraft version or needs a newer version of another mod."));
    };
    auto turnOff = [&](const QString& id, const QString& cause) {
        const QString file = d.modFiles.value(id);
        if (file.isEmpty() || d.modsToDisable.contains(file))
            return false;
        d.modsToDisable << file;
        d.plan << QObject::tr("Turn off %1 (%2)").arg(nameOf(id), QFileInfo(file).fileName());
        addOnce(causes, cause);
        return true;
    };

    for (auto it = fabricMissing.globalMatch(log); it.hasNext();) {
        const auto match = it.next();
        needs(match.captured(3), match.captured(2));
    }
    for (auto it = fabricInstall.globalMatch(log); it.hasNext();)
        needs(it.next().captured(1), QString());
    for (auto it = fabricWrong.globalMatch(log); it.hasNext();) {
        const auto match = it.next();
        const QString requester = match.captured(2);
        const QString dependency = match.captured(4);
        if (dependency == QLatin1String("minecraft")) {
            update(requester);
        } else if (dependency == QLatin1String("java")) {
            static const QRegularExpression number(QStringLiteral(R"((\d+))"));
            const int major = number.match(match.captured(3)).captured(1).toInt();
            if (major > d.javaMajor)
                d.javaMajor = major;
        } else if (dependency == QLatin1String("fabricloader") || dependency == QLatin1String("quilt_loader")) {
            addOnce(d.advice, QObject::tr("%1 needs a newer Fabric/Quilt Loader: change it in the instance's Edit → Version.")
                                  .arg(nameOf(requester, match.captured(1))));
            addOnce(causes, QObject::tr("A mod needs a newer mod loader."));
        } else {
            update(dependency);
        }
    }
    for (auto it = fabricReplace.globalMatch(log); it.hasNext();)
        update(it.next().captured(2));
    for (auto it = fabricRemove.globalMatch(log); it.hasNext();)
        turnOff(it.next().captured(2), QObject::tr("Two mods don't work together."));
    for (auto it = forgeDependency.globalMatch(log); it.hasNext();) {
        const auto match = it.next();
        const QString id = match.captured(1);
        const QString requester = match.captured(2);
        if (match.captured(4).contains(QLatin1String("MISSING"), Qt::CaseInsensitive))
            needs(id, requester);
        else if (isPlatform(id))
            update(requester);
        else
            update(id);
    }

    // Damaged files: a mod gets turned off, anything else is downloaded again.
    static const QRegularExpression zipError(
        QStringLiteral(R"(ZipException|zip END header not found|invalid LOC header|error in opening zip file|Invalid or corrupt jarfile|zip file is empty)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression jarPath(QStringLiteral(R"(((?:[A-Za-z]:)?[\\/][^\s'"\[\]]+?\.(?:jar|zip)))"));
    static const QRegularExpression fabricAnalyzing(QStringLiteral(R"(Error analyzing \[([^\]\n]+\.jar)\])"));
    const QStringList lines = log.split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (!zipError.match(line).hasMatch() && !line.contains(QLatin1String("Error analyzing [")))
            continue;
        QString path = fabricAnalyzing.match(line).captured(1);
        if (path.isEmpty())
            path = jarPath.match(line).captured(1);
        path = QDir::cleanPath(path);
        if (!modsDir.isEmpty() && !path.isEmpty() && path.startsWith(modsDir) && QFileInfo(path).isFile()) {
            if (!d.modsToDisable.contains(path)) {
                d.modsToDisable << path;
                d.plan << QObject::tr("Turn off %1: the file is damaged").arg(QFileInfo(path).fileName());
            }
            addOnce(causes, QObject::tr("A mod file is damaged."));
        } else if (zipError.match(line).hasMatch()) {
            d.files = true;
            addOnce(causes, QObject::tr("Some game files are damaged."));
        }
    }
    static const QRegularExpression missingGame(
        QStringLiteral(R"((NoClassDefFoundError|ClassNotFoundException|Could not find or load main class)[:\s]+net[./]minecraft)"));
    if (missingGame.match(log).hasMatch()) {
        d.files = true;
        addOnce(causes, QObject::tr("Some game files are missing."));
    }

    // Java too old: "class file version 65.0" is Java 21 (class version - 44).
    static const QRegularExpression classVersion(QStringLiteral(R"(class file version (\d+)(?:\.\d+)?)"));
    for (auto it = classVersion.globalMatch(log); it.hasNext();) {
        const int major = it.next().captured(1).toInt() - 44;
        if (major > d.javaMajor)
            d.javaMajor = major;
    }
    if (d.javaMajor > 0) {
        d.plan << QObject::tr("Use Java %1 or newer for this instance").arg(d.javaMajor);
        addOnce(causes, QObject::tr("The game or a mod needs Java %1 or newer.").arg(d.javaMajor));
    }

    // JVM options and memory.
    auto settings = instance->settings();
    const int maxMemory = settings->get("MaxMemAlloc").toInt();
    const int totalRam = int(HardwareInfo::totalRamMiB());
    static const QRegularExpression badOption(QStringLiteral(R"(Unrecognized (?:VM )?option[:\s]+'?([^'\s]+)'?)"));
    const auto badOptionMatch = badOption.match(log);
    if (badOptionMatch.hasMatch()) {
        d.badJvmOption = badOptionMatch.captured(1);
        d.plan << QObject::tr("Remove the Java option %1, which this Java doesn't know").arg(d.badJvmOption);
        addOnce(causes, QObject::tr("A Java option in the settings isn't understood by this Java."));
    } else if (log.contains(QLatin1String("Could not create the Java Virtual Machine")) &&
               !log.contains(QLatin1String("heap"), Qt::CaseInsensitive)) {
        d.resetJvmArgs = true;
        d.plan << QObject::tr("Go back to the default Java options");
        addOnce(causes, QObject::tr("Java didn't start with the options in the settings."));
    }
    if (log.contains(QLatin1String("Invalid maximum heap size")) || log.contains(QLatin1String("Invalid initial heap size")) ||
        log.contains(QLatin1String("Could not reserve enough space")) ||
        log.contains(QLatin1String("insufficient memory for the Java Runtime"))) {
        // Asked for more than this computer (or a 32-bit Java) can give.
        int target = totalRam > 0 ? qBound(1024, totalRam / 2, 8192) : 2048;
        target -= target % 512;
        if (maxMemory <= 0 || target < maxMemory) {
            d.memoryMiB = target;
            d.plan << QObject::tr("Lower the memory for this instance to %1 MiB").arg(target);
            addOnce(causes, QObject::tr("The game asked for more memory than Java could get."));
        }
    } else if (log.contains(QLatin1String("java.lang.OutOfMemoryError"))) {
        int target = qMax(4096, maxMemory + maxMemory / 2);
        if (totalRam > 0)
            target = qMin(target, totalRam * 3 / 5);
        target -= target % 512;
        if (target > maxMemory) {
            d.memoryMiB = target;
            d.plan << QObject::tr("Give the game more memory: %1 MiB instead of %2 MiB").arg(target).arg(maxMemory);
            addOnce(causes, QObject::tr("The game ran out of memory."));
        } else {
            addOnce(d.advice, QObject::tr("The game ran out of memory and this computer has none to spare: close other programs or remove some mods."));
            addOnce(causes, QObject::tr("The game ran out of memory."));
        }
    }

    if (looksLikeGraphicsFailure(log)) {
        d.graphics = true;
        d.plan << QObject::tr("Undo graphics tweaks that keep OpenGL from starting (Zink, forced GPU, system GLFW)");
        addOnce(causes, QObject::tr("The game couldn't open its window: OpenGL didn't start."));
        addOnce(d.advice, QObject::tr("If it still doesn't open, update your graphics driver."));
    }

    // A crash that names a mod, when nothing above explains it.
    if (d.modsToInstall.isEmpty() && d.modsToUpdate.isEmpty() && d.modsToDisable.isEmpty() && !mods.files.isEmpty()) {
        QStringList suspects;
        static const QList<QRegularExpression> patterns = {
            QRegularExpression(QStringLiteral(R"(Mixin apply for mod ([\w.\-]+) failed)")),
            QRegularExpression(QStringLiteral(R"(from mod ([\w.\-]+)\])")),
            QRegularExpression(QStringLiteral(R"(Mod loading issue for: ([\w.\-]+))")),
            QRegularExpression(QStringLiteral(R"(Caught exception from [^\n]*?\(([\w.\-]+)\))")),
            QRegularExpression(QStringLiteral(R"(Failed to create mod instance\. ModID: ([\w.\-]+))")),
        };
        for (const auto& pattern : patterns) {
            for (auto it = pattern.globalMatch(log); it.hasNext();)
                addOnce(suspects, it.next().captured(1));
        }
        // Minecraft's crash report: "Suspected Mods: Sodium (sodium), Iris (iris)" or one per line below.
        const int at = log.indexOf(QRegularExpression(QStringLiteral(R"(Suspected Mods?:)")));
        if (at >= 0) {
            static const QRegularExpression named(QStringLiteral(R"([^,\n\t]+? \(([\w.\-]+)\))"));
            QString block = log.mid(at, 600);
            block = block.left(block.indexOf(QStringLiteral("\n\n")) > 0 ? block.indexOf(QStringLiteral("\n\n")) : block.size());
            for (auto it = named.globalMatch(block); it.hasNext();)
                addOnce(suspects, it.next().captured(1));
        }
        static const QRegularExpression modFile(QStringLiteral(R"(Mod File: (\S+\.jar))"));
        for (auto it = modFile.globalMatch(log); it.hasNext();) {
            const QString path = QDir::cleanPath(it.next().captured(1));
            for (auto file = d.modFiles.begin(); file != d.modFiles.end(); ++file) {
                if (QDir::cleanPath(file.value()) == path)
                    addOnce(suspects, file.key());
            }
        }
        int disabled = 0;
        for (const QString& id : suspects) {
            if (disabled >= kMaxSuspects || isPlatform(id) || isSharedApi(id))
                continue;
            if (turnOff(id, QObject::tr("The game crashed in a mod.")))
                disabled++;
        }
        if (disabled > 0)
            addOnce(d.advice, QObject::tr("If you need the mod that was turned off, look for an update for it, then turn it back on in Edit → Mods."));
    }

    if (log.contains(QLatin1String("No space left on device"))) {
        addOnce(causes, QObject::tr("The disk is full."));
        addOnce(d.advice, QObject::tr("Free some space on the disk, then start again."));
    }

    // Nothing specific: the files most likely to be damaged are downloaded again.
    if (d.plan.isEmpty() && causes.isEmpty())
        d.files = true;
    if (d.files) {
        d.plan << QObject::tr("Download the game files and libraries again and refresh the version info");
    }
    d.plan << QObject::tr("Check that the Java in the settings still exists");

    // At most two causes, most important first (the order they were found in).
    d.cause = causes.mid(0, 2).join(QLatin1Char(' '));
    return d;
}

void repair(BaseInstance* instance, const Diagnosis& d, std::function<void(const QStringList& done, const QStringList& failed)> finished)
{
    QStringList done;
    QStringList failed;
    if (!instance) {
        finished(done, failed);
        return;
    }
    auto minecraft = dynamic_cast<MinecraftInstance*>(instance);
    auto settings = instance->settings();
    auto global = APPLICATION->settings();

    // 1. Mods that are broken, duplicated or crash the game
    for (const QString& file : d.modsToDisable) {
        if (disableFile(file))
            done << QObject::tr("Turned off %1").arg(QFileInfo(file).fileName());
        else
            failed << QObject::tr("Couldn't turn off %1").arg(QFileInfo(file).fileName());
    }

    // 2. Game files
    if (minecraft && d.files) {
        auto packProfile = minecraft->getPackProfile();
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

        // Libraries and the game jar (local libraries are the user's own files: never touched)
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

        // Natives are unpacked on every launch; a half-written folder breaks the game.
        const QString natives = minecraft->getNativePath();
        if (QDir(natives).exists() && FS::deletePath(natives))
            done << QObject::tr("Cleared the natives folder");
    }

    // 3. Java
    if (d.javaMajor > 0) {
        QString why;
        const QString java = LocalServer::findJava(QString(), d.javaMajor, &why);
        if (!java.isEmpty()) {
            settings->set("OverrideJavaLocation", true);
            settings->set("JavaPath", java);
            // Chosen on purpose for a mod that needs it, even if Minecraft asks for an older one.
            settings->set("IgnoreJavaCompatibility", true);
            done << QObject::tr("This instance now uses Java %1 (%2)").arg(LocalServer::javaMajor(java)).arg(java);
        } else {
            settings->set("OverrideJavaLocation", false);
            global->set("AutomaticJavaSwitch", true);
            global->set("AutomaticJavaDownload", true);
            failed << QObject::tr("No Java %1 or newer on this computer: automatic Java is on, and if that doesn't help install Java %1 "
                                  "(for example from adoptium.net)")
                          .arg(d.javaMajor);
        }
    }
    if (settings->get("OverrideJavaLocation").toBool() && FS::ResolveExecutable(settings->get("JavaPath").toString()).isEmpty()) {
        settings->set("OverrideJavaLocation", false);
        done << QObject::tr("This instance pointed to a Java that's gone, it now uses the global one");
    }
    if (FS::ResolveExecutable(global->get("JavaPath").toString()).isEmpty()) {
        global->set("AutomaticJavaSwitch", true);
        global->set("AutomaticJavaDownload", true);
        done << QObject::tr("Turned on automatic Java, so the right Java is picked or downloaded");
    }

    // 4. Memory and Java options, changed on this instance only
    if (d.memoryMiB > 0) {
        settings->set("OverrideMemory", true);
        settings->set("MaxMemAlloc", d.memoryMiB);
        if (settings->get("MinMemAlloc").toInt() > d.memoryMiB)
            settings->set("MinMemAlloc", qMin(512, d.memoryMiB));
        done << QObject::tr("Memory for this instance: %1 MiB").arg(d.memoryMiB);
    }
    if (!d.badJvmOption.isEmpty()) {
        const bool own = settings->get("OverrideJavaArgs").toBool();
        auto target = own ? settings : global;
        const QString before = target->get("JvmArgs").toString();
        const QString after = withoutOption(before, d.badJvmOption);
        if (after != before) {
            target->set("JvmArgs", after);
            done << QObject::tr("Removed the Java option %1").arg(d.badJvmOption);
        } else {
            failed << QObject::tr("The Java option %1 isn't in the settings: it may come from a mod pack or the system").arg(d.badJvmOption);
        }
    }
    if (d.resetJvmArgs) {
        if (settings->get("OverrideJavaArgs").toBool()) {
            settings->set("OverrideJavaArgs", false);
            done << QObject::tr("This instance uses the default Java options again");
        } else if (!global->get("JvmArgs").toString().trimmed().isEmpty()) {
            global->reset("JvmArgs");
            done << QObject::tr("Java options are back to the defaults");
        }
    }

    // 5. Graphics tweaks that stop OpenGL from starting. They're undone where they were set: on
    //    the instance if it overrides them, otherwise in the global settings.
    if (d.graphics) {
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
        if (turnOff(QStringLiteral("UseNativeGLFW"), QStringLiteral("OverrideNativeWorkarounds")))
            done << QObject::tr("Went back to the GLFW that comes with the game");
    }

    APPLICATION->metacache()->SaveNow();

    // 6. Mods from Modrinth
    PackProfile* profile = minecraft ? minecraft->getPackProfile() : nullptr;
    const QString gameVersion = profile ? profile->getComponentVersion(QStringLiteral("net.minecraft")) : QString();
    if (!minecraft || gameVersion.isEmpty() || (d.modsToInstall.isEmpty() && d.modsToUpdate.isEmpty())) {
        for (const QString& line : done)
            qDebug() << "[Repair]" << instance->name() << "-" << line;
        finished(done, failed);
        return;
    }
    auto fetch = std::make_shared<ModrinthFetch>();
    fetch->network = APPLICATION->network();
    fetch->folder = minecraft->modsRoot();
    fetch->gameVersion = gameVersion;
    fetch->loaders = modrinthLoaders(profile);
    fetch->names = d.modNames;
    for (auto it = d.modFiles.begin(); it != d.modFiles.end(); ++it)
        fetch->installed.insert(it.key());
    QDir().mkpath(fetch->folder);
    for (const QString& id : d.modsToInstall) {
        QStringList neededBy;
        for (const auto& [missing, requester] : d.requiredBy) {
            if (missing == id && d.modFiles.contains(requester))
                addOnce(neededBy, d.modFiles.value(requester));
        }
        fetch->seen.insert(id);
        fetch->queue.append({ id, QString(), neededBy });
    }
    for (const QString& id : d.modsToUpdate) {
        fetch->seen.insert(id);
        fetch->queue.append({ id, d.modFiles.value(id), {} });
    }
    fetch->done = done;
    fetch->failed = failed;
    const QString name = instance->name();
    // The pending network callbacks keep the fetch alive, so this one only holds it weakly.
    fetch->finished = [weak = std::weak_ptr<ModrinthFetch>(fetch), finished, name] {
        auto self = weak.lock();
        if (!self)
            return;
        for (const QString& line : self->done)
            qDebug() << "[Repair]" << name << "-" << line;
        finished(self->done, self->failed);
    };
    fetch->next();
}

}  // namespace InstanceRepair
