#include "SocialClient.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

#include <algorithm>
#include <tuple>

#include "minecraft/auth/AccountTokenCipher.h"
#include "minecraft/skins/XylarSkinClient.h"

namespace {
constexpr int kPresenceIntervalMs = 60 * 1000;
constexpr int kRefreshIntervalMs = 30 * 1000;

QString messageFor(int status, const QJsonObject& body)
{
    const QString error = body.value("error").toString();
    if (!error.isEmpty())
        return error;
    if (status == 0)
        return SocialClient::tr("XylarJava Social can't be reached. Check your connection.");
    return SocialClient::tr("XylarJava Social answered HTTP %1.").arg(status);
}

QList<SocialClient::Request> requestsFrom(const QJsonArray& array)
{
    QList<SocialClient::Request> out;
    for (const auto& value : array) {
        const QJsonObject row = value.toObject();
        out.append({ row.value("uuid").toString(), row.value("name").toString() });
    }
    return out;
}
}  // namespace

SocialClient::SocialClient(QNetworkAccessManager* network, const QString& dataDir, QObject* parent)
    : QObject(parent), m_network(network), m_dataDir(dataDir)
{
    m_presenceTimer.setInterval(kPresenceIntervalMs);
    connect(&m_presenceTimer, &QTimer::timeout, this, &SocialClient::sendPresence);
    m_refreshTimer.setInterval(kRefreshIntervalMs);
    connect(&m_refreshTimer, &QTimer::timeout, this, &SocialClient::refresh);
}

void SocialClient::setBackendUrl(const QString& url)
{
    QString trimmed = url.trimmed();
    while (trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);
    m_backendUrl = trimmed;
}

void SocialClient::setPlayer(const QString& name)
{
    if (name == m_name)
        return;
    if (ready())
        call("POST", QStringLiteral("/v1/offline"), {}, nullptr);

    m_generation++;
    m_name = name;
    m_uuid = name.isEmpty() ? QString() : XylarSkinClient::computeOfflineUUID(name);
    m_key = m_uuid.isEmpty() ? QString() : storedKey(m_uuid);
    m_claiming = false;
    m_loaded = false;
    m_error.clear();
    m_friends.clear();
    m_incoming.clear();
    m_outgoing.clear();
    m_invites.clear();
    m_presenceTimer.stop();
    m_refreshTimer.stop();
    emit changed();

    if (m_name.isEmpty())
        return;
    if (!ready()) {
        claim();
        return;
    }
    sendPresence();
    refresh();
    m_presenceTimer.start();
    if (m_watching)
        m_refreshTimer.start();
}

void SocialClient::setActivity(const QString& state, const QString& detail, const QString& server, const QString& version)
{
    if (state == m_state && detail == m_detail && server == m_server && version == m_version)
        return;
    m_state = state;
    m_detail = detail;
    m_server = server;
    m_version = version;
    emit changed();
    if (ready())
        sendPresence();
}

void SocialClient::setWatching(bool watching)
{
    m_watching = watching;
    if (!watching) {
        m_refreshTimer.stop();
        return;
    }
    if (ready()) {
        refresh();
        m_refreshTimer.start();
    }
}

void SocialClient::call(const QByteArray& method, const QString& path, const QJsonObject& body, Done done)
{
    if (m_backendUrl.isEmpty() || m_uuid.isEmpty())
        return;
    QNetworkRequest request{ QUrl(m_backendUrl + path) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setTransferTimeout(15000);
    if (!m_key.isEmpty()) {
        request.setRawHeader("X-Player", m_uuid.toLatin1());
        request.setRawHeader("X-Player-Key", m_key.toLatin1());
    }
    const QByteArray payload = body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_network->sendCustomRequest(request, method, payload);
    const int generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, done, generation] {
        reply->deleteLater();
        if (generation != m_generation || !done)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(status, QJsonDocument::fromJson(reply->readAll()).object());
    });
}

void SocialClient::claim()
{
    if (m_claiming)
        return;
    m_claiming = true;
    call("POST", QStringLiteral("/v1/claim"), { { "name", m_name } }, [this](int status, const QJsonObject& body) {
        m_claiming = false;
        const QString key = body.value("key").toString();
        if (status == 201 && !key.isEmpty()) {
            m_key = key;
            storeKey(m_uuid, key);
            m_error.clear();
            sendPresence();
            refresh();
            m_presenceTimer.start();
            if (m_watching)
                m_refreshTimer.start();
            emit changed();
            return;
        }
        if (status == 409)
            return setError(tr("The name %1 is already used on XylarJava Social from another install.").arg(m_name));
        setError(messageFor(status, body));
        if (status != 0 && status < 500)
            return;
        // Try again later: the backend may just be unreachable right now.
        QTimer::singleShot(kPresenceIntervalMs, this, [this, generation = m_generation] {
            if (generation == m_generation && !ready())
                claim();
        });
    });
}

void SocialClient::sendPresence()
{
    if (m_presencePending)
        return;
    m_presencePending = true;
    QTimer::singleShot(0, this, [this] {
        m_presencePending = false;
        sendPresenceNow();
    });
}

void SocialClient::sendPresenceNow()
{
    if (!ready())
        return;
    QJsonObject body{ { "state", m_state } };
    if (!m_detail.isEmpty())
        body.insert("detail", m_detail.left(80));
    if (!m_server.isEmpty())
        body.insert("server", m_server);
    if (!m_version.isEmpty())
        body.insert("version", m_version.left(32));
    call("POST", QStringLiteral("/v1/presence"), body, [this](int status, const QJsonObject& reply) {
        if (status == 401)
            setError(tr("This install's XylarJava Social key for %1 no longer works.").arg(m_name));
        else if (status != 200 && status != 429)
            setError(messageFor(status, reply));
        else if (status == 200 && reply.contains("friends"))
            applyList(reply);  // the heartbeat brings the friend list along
    });
}

void SocialClient::refresh()
{
    if (!ready())
        return;
    call("GET", QStringLiteral("/v1/friends"), {}, [this](int status, const QJsonObject& body) {
        if (status != 200) {
            if (status != 429)
                setError(messageFor(status, body));
            return;
        }
        applyList(body);
    });
}

void SocialClient::applyList(const QJsonObject& body)
{
    QList<Friend> friends;
    for (const auto& value : body.value("friends").toArray()) {
        const QJsonObject row = value.toObject();
        Friend person;
        person.uuid = row.value("uuid").toString();
        person.name = row.value("name").toString();
        person.state = row.value("state").toString(QStringLiteral("offline"));
        person.detail = row.value("detail").toString();
        person.server = row.value("server").toString();
        person.version = row.value("version").toString();
        person.seenAt = qint64(row.value("seen_at").toDouble());
        friends.append(person);
    }
    QList<Invite> invites;
    for (const auto& value : body.value("invites").toArray()) {
        const QJsonObject row = value.toObject();
        invites.append({ row.value("name").toString(), row.value("server").toString(), row.value("version").toString(),
                         row.value("title").toString(), qint64(row.value("created_at").toDouble()) });
    }
    const QList<Request> incoming = requestsFrom(body.value("incoming").toArray());
    const QList<Request> outgoing = requestsFrom(body.value("outgoing").toArray());

    // Notices only for what changed since the last answer, never for the first one.
    QList<std::tuple<Notice, QString, QString>> notices;
    if (m_loaded) {
        auto has = [](const auto& list, const QString& uuid) {
            return std::any_of(list.begin(), list.end(), [&](const auto& item) { return item.uuid == uuid; });
        };
        for (const auto& request : incoming)
            if (!has(m_incoming, request.uuid))
                notices.append({ Notice::FriendRequest, request.name, QString() });
        for (const auto& person : friends) {
            const auto before = std::find_if(m_friends.begin(), m_friends.end(), [&](const Friend& f) { return f.uuid == person.uuid; });
            if (before == m_friends.end()) {
                if (has(m_outgoing, person.uuid))
                    notices.append({ Notice::RequestAccepted, person.name, QString() });
            } else if (before->state == QLatin1String("offline") && person.state != QLatin1String("offline")) {
                notices.append({ Notice::FriendOnline, person.name, QString() });
            }
        }
        for (const auto& invite : invites) {
            const bool seen = std::any_of(m_invites.begin(), m_invites.end(), [&](const Invite& old) {
                return old.name == invite.name && old.createdAt == invite.createdAt;
            });
            if (!seen)
                notices.append({ Notice::Invite, invite.name, invite.title.isEmpty() ? invite.server : invite.title });
        }
    }

    m_friends = friends;
    m_incoming = incoming;
    m_outgoing = outgoing;
    m_invites = invites;
    m_loaded = true;
    m_error.clear();
    emit changed();
    for (const auto& [kind, name, detail] : notices)
        emit notice(kind, name, detail);
}

void SocialClient::friendAction(const QByteArray& method, const QString& path, const QString& name, const QString& okMessage)
{
    if (!ready()) {
        emit actionFinished(false, m_error.isEmpty() ? tr("XylarJava Social isn't ready yet.") : m_error);
        return;
    }
    const QJsonObject body = method == "DELETE" ? QJsonObject() : QJsonObject{ { "name", name } };
    call(method, path, body, [this, okMessage](int status, const QJsonObject& reply) {
        const bool ok = status >= 200 && status < 300;
        QString message = ok ? okMessage : messageFor(status, reply);
        if (ok && reply.value("friends").toBool())
            message = tr("You and %1 are friends now.").arg(reply.value("name").toString());
        emit actionFinished(ok, message);
        if (ok)
            refresh();
    });
}

void SocialClient::sendRequest(const QString& name)
{
    friendAction("POST", QStringLiteral("/v1/friends/request"), name, tr("Friend request sent to %1.").arg(name));
}

void SocialClient::accept(const QString& name)
{
    friendAction("POST", QStringLiteral("/v1/friends/accept"), name, tr("You and %1 are friends now.").arg(name));
}

void SocialClient::decline(const QString& name)
{
    friendAction("POST", QStringLiteral("/v1/friends/decline"), name, QString());
}

void SocialClient::remove(const QString& name)
{
    friendAction("DELETE", QStringLiteral("/v1/friends/") + QString::fromLatin1(QUrl::toPercentEncoding(name)), name,
                 tr("%1 is no longer your friend.").arg(name));
}

void SocialClient::invite(const QString& name, const QString& server, const QString& version, const QString& title)
{
    if (!ready()) {
        emit actionFinished(false, m_error.isEmpty() ? tr("XylarJava Social isn't ready yet.") : m_error);
        return;
    }
    QJsonObject body{ { "name", name }, { "server", server }, { "title", title.left(40) } };
    if (!version.isEmpty())
        body.insert("version", version);
    call("POST", QStringLiteral("/v1/invites"), body, [this, name](int status, const QJsonObject& reply) {
        const bool ok = status == 201;
        emit actionFinished(ok, ok ? tr("%1 is invited. They can join from their launcher.").arg(name) : messageFor(status, reply));
    });
}

void SocialClient::dismissInvite(const QString& name)
{
    m_invites.erase(std::remove_if(m_invites.begin(), m_invites.end(), [&](const Invite& i) { return i.name == name; }), m_invites.end());
    emit changed();
    if (ready())
        call("POST", QStringLiteral("/v1/invites/dismiss"), { { "name", name } }, nullptr);
}

void SocialClient::goOfflineNow(int timeoutMs)
{
    if (!ready() || m_backendUrl.isEmpty())
        return;
    m_presenceTimer.stop();
    m_refreshTimer.stop();
    QEventLoop loop;
    QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
    call("POST", QStringLiteral("/v1/offline"), {}, [&loop](int, const QJsonObject&) { loop.quit(); });
    loop.exec();
    m_generation++;
}

void SocialClient::setError(const QString& error)
{
    if (error == m_error)
        return;
    m_error = error;
    emit changed();
}

QString SocialClient::keysFile() const
{
    return QDir(m_dataDir).filePath(QStringLiteral("social/keys.json"));
}

QString SocialClient::storedKey(const QString& uuid) const
{
    QFile file(keysFile());
    if (!file.open(QIODevice::ReadOnly))
        return {};
    // Keys are kept per backend address, so a test server's keys never reach the real one.
    const QString stored = QJsonDocument::fromJson(file.readAll()).object().value(m_backendUrl).toObject().value(uuid).toString();
    return stored.isEmpty() ? QString() : AccountTokenCipher::decrypt(stored);
}

void SocialClient::storeKey(const QString& uuid, const QString& key)
{
    QDir().mkpath(QFileInfo(keysFile()).absolutePath());
    QJsonObject keys;
    {
        QFile file(keysFile());
        if (file.open(QIODevice::ReadOnly))
            keys = QJsonDocument::fromJson(file.readAll()).object();
    }
    QJsonObject forBackend = keys.value(m_backendUrl).toObject();
    forBackend.insert(uuid, AccountTokenCipher::encrypt(key));
    keys.insert(m_backendUrl, forBackend);
    QSaveFile out(keysFile());
    if (!out.open(QIODevice::WriteOnly))
        return;
    out.write(QJsonDocument(keys).toJson(QJsonDocument::Indented));
    if (out.commit())
        QFile::setPermissions(keysFile(), QFile::ReadOwner | QFile::WriteOwner);
}
