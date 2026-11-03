#pragma once

#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * XylarJava Social: friends and online status, from its own backend (kept apart from XylarSkins).
 *
 * A player is a Minecraft name, so offline and Microsoft accounts work the same way. The name is
 * claimed once per install; the key that comes back is stored encrypted next to the launcher data.
 * While a player is set, presence is sent every minute and the friend list is refreshed every
 * 30 seconds (only while someone is looking at it).
 */
class SocialClient : public QObject {
    Q_OBJECT

   public:
    struct Friend {
        QString uuid;
        QString name;
        QString state;  ///< launcher, playing, away or offline
        QString detail;
        QString server;   ///< address of the server they're on, when they share it
        QString version;  ///< Minecraft version they're playing
        qint64 seenAt = 0;  ///< seconds since the epoch, 0 when never seen
    };
    struct Request {
        QString uuid;
        QString name;
    };
    /// A friend asked you to join their server.
    struct Invite {
        QString name;
        QString server;
        QString version;
        QString title;
        qint64 createdAt = 0;
    };
    /// Something worth a notification, found when the friend list changed.
    enum class Notice { FriendRequest, RequestAccepted, FriendOnline, Invite };
    Q_ENUM(Notice)

    explicit SocialClient(QNetworkAccessManager* network, const QString& dataDir, QObject* parent = nullptr);

    void setBackendUrl(const QString& url);

    /// The Minecraft name to be online as; empty signs out (and tells the backend).
    void setPlayer(const QString& name);
    QString player() const { return m_name; }

    /// What the player is doing: "launcher", "playing" or "away", with a short detail for "playing",
    /// the server they're on (shared with friends so they can join) and the Minecraft version.
    void setActivity(const QString& state, const QString& detail = QString(), const QString& server = QString(), const QString& version = QString());
    QString activity() const { return m_state; }
    QString activityDetail() const { return m_detail; }

    /// Refresh the friend list on a timer while true.
    void setWatching(bool watching);

    void refresh();
    void sendRequest(const QString& name);
    void accept(const QString& name);
    void decline(const QString& name);
    void remove(const QString& name);
    /// Invite a friend to a server; they get a notification with a Join button.
    void invite(const QString& name, const QString& server, const QString& version, const QString& title);
    /// Forget the invite from @p name.
    void dismissInvite(const QString& name);

    /// Tells the backend this player went offline, waiting at most @p timeoutMs (for quitting).
    void goOfflineNow(int timeoutMs = 1500);

    bool ready() const { return !m_key.isEmpty(); }
    bool loaded() const { return m_loaded; }
    QString lastError() const { return m_error; }
    const QList<Friend>& friends() const { return m_friends; }
    const QList<Request>& incoming() const { return m_incoming; }
    const QList<Request>& outgoing() const { return m_outgoing; }
    const QList<Invite>& invites() const { return m_invites; }

   signals:
    /// The friend list, requests, readiness or the last error changed.
    void changed();
    /// A friend request, accept or remove finished; @p ok false comes with a message to show.
    void actionFinished(bool ok, const QString& message);
    /// A friend request, an accepted request, a friend coming online or an invite (not sent for
    /// what was already there when the list was first loaded). @p detail is the invite's server title.
    void notice(SocialClient::Notice kind, const QString& name, const QString& detail);

   private:
    using Done = std::function<void(int status, const QJsonObject& body)>;
    void call(const QByteArray& method, const QString& path, const QJsonObject& body, Done done);
    void claim();
    /// Sends the presence once the current event is done, so a player change and the new activity go out together.
    void sendPresence();
    void sendPresenceNow();
    void friendAction(const QByteArray& method, const QString& path, const QString& name, const QString& okMessage);
    void setError(const QString& error);
    /// Takes in a friend list answer (from GET /v1/friends or a heartbeat) and sends the notices.
    void applyList(const QJsonObject& body);
    QString keysFile() const;
    QString storedKey(const QString& uuid) const;
    void storeKey(const QString& uuid, const QString& key);

    QNetworkAccessManager* m_network = nullptr;
    QString m_dataDir;
    QString m_backendUrl;

    QString m_name;
    QString m_uuid;
    QString m_key;
    QString m_state = QStringLiteral("launcher");
    QString m_detail;
    QString m_server;
    QString m_version;
    bool m_claiming = false;
    bool m_watching = false;
    bool m_loaded = false;
    bool m_presencePending = false;
    QString m_error;
    int m_generation = 0;  ///< bumped on every player change so late answers for the old one are dropped

    QList<Friend> m_friends;
    QList<Request> m_incoming;
    QList<Request> m_outgoing;
    QList<Invite> m_invites;

    QTimer m_presenceTimer;
    QTimer m_refreshTimer;
};
