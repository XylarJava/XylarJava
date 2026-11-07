#include "LocalServer.h"

#include <QCryptographicHash>
#include <QDateTime>
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
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>
#include <memory>
#include <utility>

#ifdef Q_OS_LINUX
#include <sys/prctl.h>
#include <csignal>
#endif

#include "Application.h"
#include "BuildConfig.h"
#include "java/JavaUtils.h"
#include "FileSystem.h"
#include "SysInfo.h"
#include "java/download/ArchiveDownloadTask.h"
#include "java/download/ManifestDownloadTask.h"
#include "meta/Index.h"
#include "meta/Version.h"
#include "minecraft/VersionFile.h"
#include "java/JavaMetadata.h"
#include "net/Mode.h"
#include "settings/SettingsObject.h"
#include "PlayitTunnel.h"
#include "UpnpPortMapper.h"
#include "minecraft/auth/YggdrasilRelay.h"

namespace {
QStringList& knownPlayers()
{
    static QStringList names;
    return names;
}

const char* CONFIG_FILE = "xylarjava-server.json";
const char* JAR_FILE = "server.jar";
const int CONSOLE_LIMIT = 3000;
}  // namespace

LocalServer::LocalServer(const QString& folder, QNetworkAccessManager* network, QObject* parent)
    : QObject(parent), m_folder(folder), m_network(network)
{}

LocalServer::~LocalServer()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // Give the world a chance to save before the launcher goes away.
        m_process->write("stop\n");
        if (!m_process->waitForFinished(15000))
            m_process->kill();
    }
}

QString LocalServer::id() const
{
    return QFileInfo(m_folder).fileName();
}

bool LocalServer::load()
{
    QFile file(QDir(m_folder).filePath(QString::fromLatin1(CONFIG_FILE)));
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonObject o = QJsonDocument::fromJson(file.readAll()).object();
    m_config.name = o.value("name").toString(id());
    m_config.software = ServerSoftware::fromId(o.value("software").toString());
    m_config.minecraftVersion = o.value("minecraftVersion").toString();
    m_config.javaPath = o.value("javaPath").toString();
    m_config.memoryMb = o.value("memoryMb").toInt(2048);
    m_config.port = o.value("port").toInt(25565);
    m_config.maxPlayers = o.value("maxPlayers").toInt(10);
    m_config.gamemode = o.value("gamemode").toString(QStringLiteral("survival"));
    m_config.difficulty = o.value("difficulty").toString(QStringLiteral("normal"));
    m_config.motd = o.value("motd").toString();
    m_config.onlineMode = o.value("onlineMode").toBool(true);
    m_config.whitelist = o.value("whitelist").toBool(false);  // servers made before the whitelist existed had none
    m_config.pvp = o.value("pvp").toBool(true);
    m_config.openPort = o.value("openPort").toBool(true);
    m_config.playit = o.value("playit").toBool(false);
    m_config.eulaAccepted = o.value("eulaAccepted").toBool(false);
    m_state = QFileInfo(QDir(m_folder).filePath(QString::fromLatin1(JAR_FILE))).isFile() ? State::Stopped : State::NotInstalled;
    return !m_config.minecraftVersion.isEmpty();
}

bool LocalServer::save() const
{
    QDir().mkpath(m_folder);
    QJsonObject o;
    o["name"] = m_config.name;
    o["software"] = ServerSoftware::id(m_config.software);
    o["minecraftVersion"] = m_config.minecraftVersion;
    o["javaPath"] = m_config.javaPath;
    o["memoryMb"] = m_config.memoryMb;
    o["port"] = m_config.port;
    o["maxPlayers"] = m_config.maxPlayers;
    o["gamemode"] = m_config.gamemode;
    o["difficulty"] = m_config.difficulty;
    o["motd"] = m_config.motd;
    o["onlineMode"] = m_config.onlineMode;
    o["whitelist"] = m_config.whitelist;
    o["pvp"] = m_config.pvp;
    o["openPort"] = m_config.openPort;
    o["playit"] = m_config.playit;
    o["eulaAccepted"] = m_config.eulaAccepted;
    QSaveFile file(QDir(m_folder).filePath(QString::fromLatin1(CONFIG_FILE)));
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(o).toJson());
    return file.commit();
}

void LocalServer::setConfig(const LocalServerConfig& config)
{
    const bool reinstall = config.software != m_config.software || config.minecraftVersion != m_config.minecraftVersion;
    m_config = config;
    save();
    if (reinstall && !isRunning()) {
        QFile::remove(QDir(m_folder).filePath(QString::fromLatin1(JAR_FILE)));
        setState(State::NotInstalled);
    } else if (m_state != State::NotInstalled && !isRunning()) {
        writeServerFiles();
    }
    emit stateChanged();
}

QString LocalServer::statusText() const
{
    switch (m_state) {
        case State::NotInstalled:
            return tr("Not downloaded yet");
        case State::Installing:
            return tr("Downloading...");
        case State::Stopped:
            return tr("Stopped");
        case State::Starting:
            return tr("Starting...");
        case State::Running:
            return tr("Online");
        case State::Stopping:
            return tr("Stopping...");
    }
    return QString();
}

QString LocalServer::lanIp() const
{
    return UpnpPortMapper::localAddress().toString();
}

QString LocalServer::joinAddress(const QString& ip) const
{
    if (m_config.port == 25565)
        return ip;
    return QStringLiteral("%1:%2").arg(ip).arg(m_config.port);
}

QString LocalServer::playitAddress() const
{
    return m_config.playit && m_playit ? m_playit->addressFor(m_config.port) : QString();
}

void LocalServer::setPlayit(PlayitTunnel* playit)
{
    m_playit = playit;
    connect(m_playit, &PlayitTunnel::changed, this, &LocalServer::addressesChanged);
}

bool LocalServer::validPlayerName(const QString& name)
{
    static const QRegularExpression valid(QStringLiteral("^[A-Za-z0-9_]{1,16}$"));
    return valid.match(name).hasMatch();
}

void LocalServer::setKnownPlayers(const QStringList& names)
{
    knownPlayers() = names;
}

QString LocalServer::knownSpelling(const QString& name)
{
    for (const QString& known : knownPlayers()) {
        if (known.compare(name, Qt::CaseInsensitive) == 0)
            return known;
    }
    return name;
}

QString LocalServer::offlineUuid(const QString& name)
{
    // Java's UUID.nameUUIDFromBytes: MD5 of the bytes, then version 3 and the IETF variant.
    QByteArray hash = QCryptographicHash::hash(QStringLiteral("OfflinePlayer:%1").arg(name).toUtf8(), QCryptographicHash::Md5);
    hash[6] = char((hash[6] & 0x0f) | 0x30);
    hash[8] = char((hash[8] & 0x3f) | 0x80);
    return QUuid::fromRfc4122(hash).toString(QUuid::WithoutBraces);
}

namespace {
QJsonArray readWhitelist(const QString& folder)
{
    QFile file(QDir(folder).filePath(QStringLiteral("whitelist.json")));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).array();
}

bool writeWhitelist(const QString& folder, const QJsonArray& entries)
{
    QDir().mkpath(folder);
    QSaveFile file(QDir(folder).filePath(QStringLiteral("whitelist.json")));
    return file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(entries).toJson()) >= 0 && file.commit();
}

QString dashedUuid(const QString& id)
{
    if (id.size() != 32)
        return id;
    return QStringLiteral("%1-%2-%3-%4-%5").arg(id.mid(0, 8), id.mid(8, 4), id.mid(12, 4), id.mid(16, 4), id.mid(20));
}

QString mojangIdsPath(const QString& folder)
{
    return QDir(folder).filePath(QStringLiteral("xylarjava-mojang-ids.json"));
}

/// lowercase name -> { "id": Mojang UUID or "" when there's no Microsoft account, "at": when asked }
QJsonObject readMojangIds(const QString& folder)
{
    QFile file(mojangIdsPath(folder));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}

QString mojangIdFor(const QJsonObject& ids, const QString& name)
{
    return ids.value(name.toLower()).toObject().value("id").toString();
}

QStringList listedNames(const QString& folder)
{
    QStringList names;
    for (const char* list : { "whitelist.json", "ops.json", "banned-players.json" }) {
        QFile file(QDir(folder).filePath(QString::fromLatin1(list)));
        if (file.open(QIODevice::ReadOnly)) {
            for (const QJsonValue& value : QJsonDocument::fromJson(file.readAll()).array())
                names << value.toObject().value("name").toString();
        }
    }
    return names;
}

/**
 * Puts the whitelist, ops and bans on the UUIDs players really join with.
 *
 * Offline, everyone (Microsoft accounts included) joins with the UUID made from their name. The
 * server's own "whitelist add" and "op" looked names up at Mojang and saved the Microsoft UUID, so
 * those players were turned away.
 *
 * In mixed mode (@p mojangIds given) a Microsoft account joins with its Mojang UUID and an offline
 * account with the offline one, so every name is listed with both: whoever plays as that name gets in.
 */
void fixPlayerLists(const QString& folder, const QJsonObject* mojangIds)
{
    for (const char* list : { "whitelist.json", "ops.json", "banned-players.json" }) {
        const QString path = QDir(folder).filePath(QString::fromLatin1(list));
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonArray entries = QJsonDocument::fromJson(file.readAll()).array();
        file.close();
        QJsonArray fixed;
        QStringList seen;
        for (const QJsonValue& value : entries) {
            QJsonObject entry = value.toObject();
            // Earlier commands may have saved the name in lowercase: spell it the way the player does.
            const QString name = LocalServer::knownSpelling(entry.value("name").toString());
            if (name.isEmpty() || seen.contains(name, Qt::CaseInsensitive))
                continue;
            seen << name;
            entry.insert("name", name);
            entry.insert("uuid", LocalServer::offlineUuid(name));
            fixed.append(entry);
            const QString mojang = mojangIds ? mojangIdFor(*mojangIds, name) : QString();
            if (!mojang.isEmpty()) {
                entry.insert("uuid", mojang);
                fixed.append(entry);
            }
        }
        if (fixed == entries)
            continue;
        QSaveFile out(path);
        if (out.open(QIODevice::WriteOnly) && out.write(QJsonDocument(fixed).toJson()) >= 0)
            out.commit();
    }

    // The server answers "op", "whitelist add" and "ban" from usercache.json before anything else,
    // and without it an offline server writes the name in lowercase, which is another UUID.
    // So it starts out knowing everyone on its lists and everyone the launcher knows, spelled right.
    // In mixed mode a name with a Microsoft account is cached with its Mojang UUID, like the server
    // itself would (an "op" for someone not online then goes to the Microsoft account).
    QStringList names = knownPlayers() + listedNames(folder);
    const QString cachePath = QDir(folder).filePath(QStringLiteral("usercache.json"));
    QJsonArray cached;
    {
        QFile cache(cachePath);
        if (cache.open(QIODevice::ReadOnly))
            cached = QJsonDocument::fromJson(cache.readAll()).array();
    }
    for (const QJsonValue& value : cached) {
        const QJsonObject entry = value.toObject();
        const QString name = entry.value("name").toString();
        const QString uuid = entry.value("uuid").toString();
        if (uuid == LocalServer::offlineUuid(name) || (mojangIds && uuid == mojangIdFor(*mojangIds, name)))
            names << name;
    }
    // Same format the server writes: "2026-12-03 02:48:57 +0100".
    const QDateTime later = QDateTime::currentDateTime().addMonths(1);
    const int offset = later.offsetFromUtc() / 60;
    const QString expires = later.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss ")) +
                            QStringLiteral("%1%2%3").arg(offset < 0 ? '-' : '+').arg(qAbs(offset) / 60, 2, 10, QLatin1Char('0')).arg(qAbs(offset) % 60, 2, 10, QLatin1Char('0'));
    QJsonArray seeded;
    QStringList seen;
    for (const QString& name : names) {
        if (!LocalServer::validPlayerName(name) || seen.contains(name, Qt::CaseInsensitive))
            continue;
        seen << name;
        const QString mojang = mojangIds ? mojangIdFor(*mojangIds, name) : QString();
        seeded.append(QJsonObject{ { "uuid", mojang.isEmpty() ? LocalServer::offlineUuid(name) : mojang }, { "name", name }, { "expiresOn", expires } });
    }
    QSaveFile out(cachePath);
    if (out.open(QIODevice::WriteOnly) && out.write(QJsonDocument(seeded).toJson()) >= 0)
        out.commit();
}
/**
 * A Microsoft account played this world offline (under its offline UUID) before mixed mode: give
 * its Microsoft UUID the same inventory, advancements and stats, so nothing looks lost.
 */
void copyOfflinePlayerData(const QString& folder, const QJsonObject& mojangIds)
{
    const QString world = QDir(folder).filePath(QStringLiteral("world"));
    // 26.1 keeps players in world/players/..., older versions straight in world/.
    const QList<QPair<QString, QString>> places = { { "players/data", ".dat" },  { "players/advancements", ".json" },
                                                    { "players/stats", ".json" }, { "playerdata", ".dat" },
                                                    { "advancements", ".json" },  { "stats", ".json" } };
    QStringList names = listedNames(folder);
    names.removeDuplicates();
    for (const QString& name : names) {
        const QString mojang = mojangIdFor(mojangIds, name);
        if (mojang.isEmpty())
            continue;
        for (const auto& [dir, ext] : places) {
            const QString from = QDir(world).filePath(dir + "/" + LocalServer::offlineUuid(name) + ext);
            const QString to = QDir(world).filePath(dir + "/" + mojang + ext);
            if (QFile::exists(from) && !QFile::exists(to))
                QFile::copy(from, to);
        }
    }
}
}  // namespace

QStringList LocalServer::allowedPlayers() const
{
    QStringList names;
    for (const QJsonValue& entry : readWhitelist(m_folder)) {
        const QString name = entry.toObject().value("name").toString();
        // In mixed mode a name is there twice (offline and Microsoft UUID).
        if (!name.isEmpty() && !names.contains(name, Qt::CaseInsensitive))
            names << name;
    }
    return names;
}

void LocalServer::allowPlayer(const QString& typed, std::function<void(const QString& error)> done)
{
    // "mrmariixx" and "Mrmariixx" are two players offline: use the spelling the launcher knows.
    const QString name = m_config.onlineMode ? typed : knownSpelling(typed);
    if (!validPlayerName(name))
        return done(tr("\"%1\" isn't a Minecraft name: 1 to 16 letters, digits or _.").arg(name));
    if (allowedPlayers().contains(name, Qt::CaseInsensitive))
        return done(QString());
    if (m_state == State::Running && m_config.onlineMode) {
        // The running server keeps its own list: let it add the player (it writes whitelist.json).
        sendCommand(QStringLiteral("whitelist add %1").arg(name));
        QTimer::singleShot(800, this, [done] { done(QString()); });
        return;
    }
    auto write = [this, name, done](const QStringList& uuids) {
        QJsonArray entries = readWhitelist(m_folder);
        for (const QString& uuid : uuids)
            entries.append(QJsonObject{ { "uuid", uuid }, { "name", name } });
        if (!writeWhitelist(m_folder, entries))
            return done(tr("Couldn't write the whitelist in %1.").arg(m_folder));
        if (m_state == State::Running)
            sendCommand(QStringLiteral("whitelist reload"));
        done(QString());
    };
    if (!m_config.onlineMode && !mixedMode())
        return write({ offlineUuid(name) });
    if (!m_config.onlineMode) {
        // Mixed mode: the name gets in both as an offline account and as a Microsoft account.
        lookUpMojangIds({ name }, [this, name, write] {
            QStringList uuids{ offlineUuid(name) };
            const QString mojang = mojangIdFor(readMojangIds(m_folder), name);
            if (!mojang.isEmpty())
                uuids << mojang;
            write(uuids);
        });
        return;
    }
    // Online mode knows players by their Mojang UUID.
    QNetworkRequest request(QUrl(QStringLiteral("https://api.mojang.com/users/profiles/minecraft/%1").arg(name)));
    request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, name, write, done] {
        reply->deleteLater();
        const QString id = QJsonDocument::fromJson(reply->readAll()).object().value("id").toString();
        if (id.size() != 32)
            return done(tr("There's no Microsoft account called %1. On a server in online mode only those can join.").arg(name));
        write({ dashedUuid(id) });
    });
}

QString LocalServer::injectorJar()
{
    const QString jar = APPLICATION->getJarPath(QStringLiteral("authlib-injector.jar"));
    return !jar.isEmpty() && QFile::exists(jar) ? jar : QString();
}

QString LocalServer::skinBackendUrl()
{
    QString url = APPLICATION->settings()->get("XylarSkinBackendUrl").toString().trimmed();
    while (url.endsWith(QLatin1Char('/')))
        url.chop(1);
    return url;
}

bool LocalServer::mixedMode() const
{
    return !m_config.onlineMode && !injectorJar().isEmpty() && !skinBackendUrl().isEmpty();
}

void LocalServer::lookUpMojangIds(const QStringList& names, std::function<void()> then)
{
    const QJsonObject ids = readMojangIds(m_folder);
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    QStringList missing;
    for (const QString& name : names) {
        if (!validPlayerName(name) || missing.contains(name, Qt::CaseInsensitive))
            continue;
        const QJsonObject known = ids.value(name.toLower()).toObject();
        // A name with no Microsoft account is asked again after a week: someone may have taken it.
        if (known.contains("id") && (!known.value("id").toString().isEmpty() || now - qint64(known.value("at").toDouble()) < 7 * 24 * 3600))
            continue;
        missing << name;
    }
    if (missing.isEmpty())
        return then();
    auto pending = std::make_shared<int>(int(missing.size()));
    for (const QString& name : missing) {
        QNetworkRequest request(QUrl(QStringLiteral("https://api.mojang.com/users/profiles/minecraft/%1").arg(name)));
        request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
        request.setTransferTimeout(8000);
        QNetworkReply* reply = m_network->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply, name, pending, then, now] {
            reply->deleteLater();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QString id = QJsonDocument::fromJson(reply->readAll()).object().value("id").toString();
            // Only a clear answer is remembered; a timeout or a rate limit is asked again next time.
            if ((status == 200 && id.size() == 32) || status == 204 || status == 404) {
                QJsonObject ids = readMojangIds(m_folder);
                ids.insert(name.toLower(), QJsonObject{ { "id", status == 200 ? dashedUuid(id) : QString() }, { "at", double(now) } });
                QSaveFile out(mojangIdsPath(m_folder));
                if (out.open(QIODevice::WriteOnly) && out.write(QJsonDocument(ids).toJson()) >= 0)
                    out.commit();
            }
            if (--*pending == 0)
                then();
        });
    }
}

void LocalServer::disallowPlayer(const QString& name)
{
    if (m_state == State::Running && m_config.onlineMode) {
        sendCommand(QStringLiteral("whitelist remove %1").arg(name));
        return;
    }
    QJsonArray kept;
    for (const QJsonValue& entry : readWhitelist(m_folder)) {
        if (entry.toObject().value("name").toString().compare(name, Qt::CaseInsensitive) != 0)
            kept.append(entry);
    }
    writeWhitelist(m_folder, kept);
    // Reloading also turns away the player if they're on right now (enforce-whitelist).
    if (m_state == State::Running)
        sendCommand(QStringLiteral("whitelist reload"));
}

QString LocalServer::addonsPath() const
{
    const QString folder = ServerSoftware::addonsFolder(m_config.software);
    return folder.isEmpty() ? QString() : QDir(m_folder).filePath(folder);
}

void LocalServer::setState(State state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged();
}

void LocalServer::appendConsole(const QString& line)
{
    m_console << line;
    if (m_console.size() > CONSOLE_LIMIT)
        m_console.erase(m_console.begin(), m_console.begin() + (m_console.size() - CONSOLE_LIMIT));
    emit consoleLine(line);
}

void LocalServer::install()
{
    if (m_state == State::Installing || isRunning())
        return;
    QDir().mkpath(m_folder);
    setState(State::Installing);
    appendConsole(tr("[XylarJava] Looking up %1 %2...").arg(ServerSoftware::displayName(m_config.software), m_config.minecraftVersion));
    ServerSoftware::resolveJar(m_network, m_config.software, m_config.minecraftVersion, this, [this](const QUrl& url, const QString& error) {
        if (!error.isEmpty()) {
            appendConsole(tr("[XylarJava] %1").arg(error));
            m_startWhenInstalled = false;
            setState(State::NotInstalled);
            emit failed(error);
            return;
        }
        appendConsole(tr("[XylarJava] Downloading %1").arg(url.toString()));
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("%1 (https://github.com/XylarJava/XylarJava)").arg(BuildConfig.USER_AGENT));
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply* reply = m_network->get(request);
        auto part = new QFile(QDir(m_folder).filePath(QString::fromLatin1(JAR_FILE) + QStringLiteral(".part")), reply);
        if (!part->open(QIODevice::WriteOnly)) {
            reply->abort();
            appendConsole(tr("[XylarJava] Can't write to %1").arg(m_folder));
            m_startWhenInstalled = false;
            setState(State::NotInstalled);
            emit failed(tr("Can't write to %1").arg(m_folder));
            return;
        }
        connect(reply, &QNetworkReply::readyRead, this, [reply, part] { part->write(reply->readAll()); });
        connect(reply, &QNetworkReply::downloadProgress, this, &LocalServer::installProgress);
        connect(reply, &QNetworkReply::finished, this, [this, reply, part] {
            reply->deleteLater();
            part->write(reply->readAll());
            part->close();
            const QString jar = QDir(m_folder).filePath(QString::fromLatin1(JAR_FILE));
            if (reply->error() != QNetworkReply::NoError || part->size() < 1024) {
                part->remove();
                const QString message = tr("Download failed: %1").arg(reply->errorString());
                appendConsole(tr("[XylarJava] %1").arg(message));
                m_startWhenInstalled = false;
                setState(State::NotInstalled);
                emit failed(message);
                return;
            }
            QFile::remove(jar);
            part->rename(jar);
            writeServerFiles();
            setState(State::Stopped);
            if (std::exchange(m_startWhenInstalled, false))
                start();
            else
                appendConsole(tr("[XylarJava] Ready. Press Start to open the server."));
        });
    });
}

void LocalServer::writeServerFiles()
{
    if (!m_config.onlineMode) {
        const QJsonObject ids = readMojangIds(m_folder);
        const bool mixed = mixedMode();
        fixPlayerLists(m_folder, mixed ? &ids : nullptr);
        if (mixed)
            copyOfflinePlayerData(m_folder, ids);
    }

    // eula.txt only says true when the user ticked the box in XylarJava.
    QFile eula(QDir(m_folder).filePath(QStringLiteral("eula.txt")));
    if (eula.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        eula.write("# Accepted in XylarJava: https://aka.ms/MinecraftEULA\n");
        eula.write(m_config.eulaAccepted ? "eula=true\n" : "eula=false\n");
    }

    // Merge our values into server.properties, keeping everything else the user changed.
    const QString path = QDir(m_folder).filePath(QStringLiteral("server.properties"));
    QStringList lines;
    QFile in(path);
    if (in.open(QIODevice::ReadOnly))
        lines = QString::fromUtf8(in.readAll()).split('\n');
    in.close();
    const QString motd = m_config.motd.isEmpty() ? tr("%1 on XylarJava").arg(m_config.name) : m_config.motd;
    const QList<QPair<QString, QString>> values = {
        { QStringLiteral("server-port"), QString::number(m_config.port) },
        { QStringLiteral("max-players"), QString::number(m_config.maxPlayers) },
        { QStringLiteral("gamemode"), m_config.gamemode },
        { QStringLiteral("difficulty"), m_config.difficulty },
        // Mixed mode is online mode with logins checked by XylarSkins: it lets offline accounts in
        // too and sends everyone's skin, which an offline server can't do.
        { QStringLiteral("online-mode"), m_config.onlineMode || mixedMode() ? QStringLiteral("true") : QStringLiteral("false") },
        { QStringLiteral("pvp"), m_config.pvp ? QStringLiteral("true") : QStringLiteral("false") },
        { QStringLiteral("white-list"), m_config.whitelist ? QStringLiteral("true") : QStringLiteral("false") },
        // Taking someone off the list while they play kicks them out too.
        { QStringLiteral("enforce-whitelist"), m_config.whitelist ? QStringLiteral("true") : QStringLiteral("false") },
        { QStringLiteral("motd"), motd },
        // Offline accounts have no chat signing key: with this on they couldn't chat next to Microsoft ones.
        { QStringLiteral("enforce-secure-profile"), m_config.onlineMode ? QStringLiteral("true") : QStringLiteral("false") },
    };
    for (const auto& kv : values) {
        bool found = false;
        for (QString& line : lines) {
            if (line.startsWith(kv.first + '=')) {
                line = kv.first + '=' + kv.second;
                found = true;
            }
        }
        if (!found)
            lines << kv.first + '=' + kv.second;
    }
    while (!lines.isEmpty() && lines.last().trimmed().isEmpty())
        lines.removeLast();
    QSaveFile out(path);
    if (out.open(QIODevice::WriteOnly)) {
        out.write(lines.join('\n').toUtf8() + '\n');
        out.commit();
    }
    if (!addonsPath().isEmpty())
        QDir().mkpath(addonsPath());
}

int LocalServer::javaMajor(const QString& javaPath)
{
    if (javaPath.isEmpty())
        return 0;
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.setProcessEnvironment(CleanEnviroment());
    process.start(javaPath, { QStringLiteral("-version") });
    if (!process.waitForFinished(5000)) {
        process.kill();
        return 0;
    }
    static const QRegularExpression version(QStringLiteral("version \"(\\d+)(?:\\.(\\d+))?"));
    const auto match = version.match(QString::fromLocal8Bit(process.readAll()));
    if (!match.hasMatch())
        return 0;
    const int first = match.captured(1).toInt();
    return first == 1 ? match.captured(2).toInt() : first;
}

QString LocalServer::findJava(const QString& configured, int requiredMajor, QString* why)
{
    QStringList candidates;
    if (!configured.isEmpty())
        candidates << configured;
    const QString launcherJava = APPLICATION->settings()->get("JavaPath").toString();
    if (!launcherJava.isEmpty())
        candidates << launcherJava;
    JavaUtils utils;
    candidates << utils.FindJavaPaths();
    const QString onPath = QStandardPaths::findExecutable(QStringLiteral("java"));
    if (!onPath.isEmpty())
        candidates << onPath;
    candidates.removeDuplicates();

    QString best;
    int bestMajor = 0;
    for (const QString& candidate : candidates) {
        const int major = javaMajor(candidate);
        if (major >= requiredMajor && (bestMajor == 0 || major < bestMajor)) {
            best = candidate;
            bestMajor = major;
        }
        if (major == requiredMajor)
            break;
    }
    if (best.isEmpty() && why)
        *why = tr("This server needs Java %1 or newer and none was found. Install it (for example from adoptium.net) "
                  "or pick one in the server's settings.")
                   .arg(requiredMajor);
    return best;
}

void LocalServer::start()
{
    if (isRunning() || m_state == State::Installing)
        return;
    if (m_state == State::NotInstalled) {
        m_startWhenInstalled = true;
        install();
        return;
    }
    if (!m_config.eulaAccepted) {
        emit failed(tr("Accept the Minecraft EULA in the server's settings first."));
        return;
    }
    QString why;
    const int requiredJava = ServerSoftware::requiredJava(m_config.minecraftVersion);
    const QString java = findJava(m_config.javaPath, requiredJava, &why);
    if (java.isEmpty()) {
        if (!m_javaTask && !SysInfo::getSupportedJavaArchitecture().isEmpty()) {
            downloadJava(requiredJava);
            return;
        }
        appendConsole(tr("[XylarJava] %1").arg(why));
        emit failed(why);
        return;
    }
    // Starting now, so a second Start does nothing while Microsoft accounts are looked up.
    setState(State::Starting);
    auto go = [this, java] {
        if (m_state == State::Starting)
            launch(java);
    };
    if (mixedMode())
        lookUpMojangIds(knownPlayers() + listedNames(m_folder), go);
    else
        go();
}

void LocalServer::launch(const QString& java)
{
    writeServerFiles();

    if (!m_process) {
        m_process = new QProcess(this);
        m_process->setProcessChannelMode(QProcess::MergedChannels);
        m_process->setProcessEnvironment(CleanEnviroment());
#ifdef Q_OS_LINUX
        // If the launcher quits or crashes, the server gets SIGTERM: it saves the world and stops
        // instead of running on where nobody can see or stop it (and keeping the port busy).
        m_process->setChildProcessModifier([] { ::prctl(PR_SET_PDEATHSIG, SIGTERM); });
#endif
        connect(m_process, &QProcess::readyRead, this, &LocalServer::onOutput);
        connect(m_process, &QProcess::finished, this, &LocalServer::onFinished);
        connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
                appendConsole(tr("[XylarJava] Java couldn't be started."));
                setState(State::Stopped);
            }
        });
    }
    m_process->setWorkingDirectory(m_folder);
    QStringList args = { QStringLiteral("-Xms%1M").arg(qMin(1024, m_config.memoryMb)), QStringLiteral("-Xmx%1M").arg(m_config.memoryMb) };
    if (mixedMode()) {
        // Logins go through XylarSkins: Microsoft accounts are passed on to Mojang, offline accounts
        // the launcher vouched for get in with their offline UUID, and both come with their skin.
        QString jar = injectorJar();
#ifdef Q_OS_WIN
        jar = FS::getPathNameInLocal8bit(jar);
#endif
        // The launcher's relay checks Microsoft accounts with Mojang (which turns away the backend).
        const QString relay = YggdrasilRelay::urlFor(skinBackendUrl());
        args << QStringLiteral("-javaagent:%1=%2").arg(jar, relay.isEmpty() ? skinBackendUrl() : relay);
        // Cloudflare turns away Java's default User-Agent.
        args << QStringLiteral("-Dhttp.agent=XylarJava");
    } else if (!m_config.onlineMode) {
        // In offline mode "whitelist add", "op" and "ban" would look names up at Mojang and use the
        // Microsoft UUID, which isn't the one the player joins with. With no Mojang to ask, the server
        // takes names from usercache.json (filled in by writeServerFiles) and gives everyone, offline
        // and Microsoft accounts alike, the UUID they really join with.
        for (const char* host : { "discovery", "auth", "account", "session", "services", "profiles" })
            args << QStringLiteral("-Dminecraft.api.%1.host=https://nope.invalid").arg(QString::fromLatin1(host));
    }
    args << QStringLiteral("-jar") << QString::fromLatin1(JAR_FILE) << QStringLiteral("nogui");
    appendConsole(tr("[XylarJava] Starting with %1 (Java %2), %3 MB of memory").arg(java).arg(javaMajor(java)).arg(m_config.memoryMb));
    m_portOpen = false;
    m_publicIp.clear();
    m_networkMessage.clear();
    emit addressesChanged();
    m_process->start(java, args);
}

void LocalServer::onOutput()
{
    m_partialLine += m_process->readAll();
    int newline;
    while ((newline = m_partialLine.indexOf('\n')) >= 0) {
        QString line = QString::fromUtf8(m_partialLine.left(newline));
        m_partialLine.remove(0, newline + 1);
        if (line.endsWith('\r'))
            line.chop(1);
        appendConsole(line);
        // "Done (4.2s)! For help, type "help"" means players can join.
        if (m_state == State::Starting && line.contains(QLatin1String("Done (")) && line.contains(QLatin1String("help"))) {
            setState(State::Running);
            openPort();
        }
    }
}

void LocalServer::openPort()
{
    if (m_config.playit && m_playit) {
        // Friends come in through playit.gg: don't open the router, the home IP stays private.
        m_usingPlayit = true;
        m_playit->acquire();
        emit addressesChanged();
        return;
    }
    if (!m_config.openPort) {
        m_networkMessage = tr("Automatic port opening is off. Friends on your Wi-Fi can join with the local address.");
        emit addressesChanged();
        return;
    }
    if (!m_upnp) {
        m_upnp = new UpnpPortMapper(m_network, this);
        connect(m_upnp, &UpnpPortMapper::finished, this, [this](bool ok, const QString& address, const QString& message) {
            m_portOpen = ok;
            m_publicIp = address;
            m_networkMessage = message;
            appendConsole(tr("[XylarJava] %1").arg(message));
            emit addressesChanged();
        });
    }
    m_networkMessage = tr("Opening the port on your router...");
    emit addressesChanged();
    m_upnp->open(quint16(m_config.port), tr("XylarJava server %1").arg(m_config.name));
}

void LocalServer::stop()
{
    if (!m_process || m_process->state() == QProcess::NotRunning)
        return;
    setState(State::Stopping);
    m_process->write("stop\n");
    QTimer::singleShot(30000, this, [this] {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            appendConsole(tr("[XylarJava] The server didn't stop in 30 seconds, closing it."));
            m_process->kill();
        }
    });
}

void LocalServer::kill()
{
    if (m_process && m_process->state() != QProcess::NotRunning)
        m_process->kill();
}

void LocalServer::sendCommand(const QString& command)
{
    if (!m_process || m_process->state() != QProcess::Running || command.trimmed().isEmpty())
        return;
    QString text = command.trimmed();
    if (text.startsWith('/'))
        text.remove(0, 1);
    if (!m_config.onlineMode) {
        // The whitelist is kept by XylarJava offline (the server would lowercase new names, which
        // changes their UUID), so typed "whitelist add/remove" go through the same path as the page.
        static const QRegularExpression whitelistCommand(QStringLiteral("^whitelist\\s+(add|remove)\\s+([A-Za-z0-9_]{1,16})$"),
                                                         QRegularExpression::CaseInsensitiveOption);
        const auto match = whitelistCommand.match(text);
        if (match.hasMatch()) {
            appendConsole(QStringLiteral("> ") + text);
            const QString name = match.captured(2);
            if (match.captured(1).compare(QLatin1String("add"), Qt::CaseInsensitive) == 0) {
                allowPlayer(name, [this, name](const QString& error) {
                    appendConsole(error.isEmpty() ? tr("[XylarJava] %1 can join now.").arg(knownSpelling(name)) : tr("[XylarJava] %1").arg(error));
                    emit whitelistChanged();
                });
            } else {
                disallowPlayer(name);
                appendConsole(tr("[XylarJava] %1 is off the whitelist.").arg(name));
                emit whitelistChanged();
            }
            return;
        }
    }
    appendConsole(QStringLiteral("> ") + text);
    m_process->write(text.toUtf8() + '\n');
}

void LocalServer::downloadJava(int major)
{
    setState(State::Installing);
    appendConsole(tr("[XylarJava] Java %1 isn't installed yet: downloading it (only the first time)...").arg(major));
    auto fail = [this, major](const QString& reason) {
        m_javaTask.reset();
        const QString message = tr("Java %1 couldn't be downloaded (%2). Install it (for example from adoptium.net) or pick one "
                                   "in the server's settings.")
                                    .arg(major)
                                    .arg(reason);
        appendConsole(tr("[XylarJava] %1").arg(message));
        setState(State::Stopped);
        emit failed(message);
    };
    const QString uid = QStringLiteral("net.minecraft.java");
    const QString version = QStringLiteral("java%1").arg(major);
    m_javaTask = APPLICATION->metadataIndex()->loadVersion(uid, version, Net::Mode::Online);
    connect(m_javaTask.get(), &Task::failed, this, fail);
    connect(m_javaTask.get(), &Task::succeeded, this, [this, uid, version, fail] {
        auto meta = APPLICATION->metadataIndex()->getLoadedVersion(uid, version);
        const QString arch = SysInfo::getSupportedJavaArchitecture();
        Java::MetadataPtr pick;
        if (meta && meta->data()) {
            for (const auto& runtime : meta->data()->runtimes) {
                if (runtime->runtimeOS != arch || runtime->downloadType == Java::DownloadType::Unknown)
                    continue;
                // Mojang's own builds ("java-runtime-...") first, like the game uses.
                if (!pick || (runtime->m_name.startsWith(QLatin1String("java-runtime")) && !pick->m_name.startsWith(QLatin1String("java-runtime"))))
                    pick = runtime;
            }
        }
        if (!pick)
            return fail(tr("none for this system"));
        const QString target = QDir(APPLICATION->javaPath()).absoluteFilePath(pick->m_name);
        if (pick->downloadType == Java::DownloadType::Manifest)
            m_javaTask = makeShared<Java::ManifestDownloadTask>(pick->url, target, pick->checksumType, pick->checksumHash);
        else
            m_javaTask = makeShared<Java::ArchiveDownloadTask>(pick->url, target, pick->checksumType, pick->checksumHash);
        connect(m_javaTask.get(), &Task::progress, this, &LocalServer::installProgress);
        connect(m_javaTask.get(), &Task::failed, this, [target, fail](const QString& reason) {
            FS::deletePath(target);
            fail(reason);
        });
        connect(m_javaTask.get(), &Task::succeeded, this, [this] {
            appendConsole(tr("[XylarJava] Java is ready."));
            setState(State::Stopped);
            start();
            m_javaTask.reset();
        });
        m_javaTask->start();
    });
    m_javaTask->start();
}

void LocalServer::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_partialLine.isEmpty()) {
        appendConsole(QString::fromUtf8(m_partialLine));
        m_partialLine.clear();
    }
    if (m_upnp)
        m_upnp->close();
    if (m_usingPlayit) {
        m_usingPlayit = false;
        m_playit->release();
    }
    m_portOpen = false;
    m_publicIp.clear();
    m_networkMessage.clear();
    emit addressesChanged();
    if (status == QProcess::CrashExit || (exitCode != 0 && m_state != State::Stopping))
        appendConsole(tr("[XylarJava] The server stopped unexpectedly (exit code %1). The lines above usually say why.").arg(exitCode));
    else
        appendConsole(tr("[XylarJava] Server stopped."));
    setState(State::Stopped);
}
