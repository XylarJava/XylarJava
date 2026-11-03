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
        QString server;
        qint64 seenAt = 0;  ///< seconds since the epoch, 0 when never seen
    };
    struct Request {
        QString uuid;
        QString name;
    };

    explicit SocialClient(QNetworkAccessManager* network, const QString& dataDir, QObject* parent = nullptr);

    void setBackendUrl(const QString& url);

    /// The Minecraft name to be online as; empty signs out (and tells the backend).
    void setPlayer(const QString& name);
    QString player() const { return m_name; }

    /// What the player is doing: "launcher", "playing" or "away", with a short detail for "playing".
    void setActivity(const QString& state, const QString& detail = QString());
    QString activity() const { return m_state; }
    QString activityDetail() const { return m_detail; }

    /// Refresh the friend list on a timer while true.
    void setWatching(bool watching);

    void refresh();
    void sendRequest(const QString& name);
    void accept(const QString& name);
    void decline(const QString& name);
    void remove(const QString& name);

    /// Tells the backend this player went offline, waiting at most @p timeoutMs (for quitting).
    void goOfflineNow(int timeoutMs = 1500);

    bool ready() const { return !m_key.isEmpty(); }
    bool loaded() const { return m_loaded; }
    QString lastError() const { return m_error; }
    const QList<Friend>& friends() const { return m_friends; }
    const QList<Request>& incoming() const { return m_incoming; }
    const QList<Request>& outgoing() const { return m_outgoing; }

   signals:
    /// The friend list, requests, readiness or the last error changed.
    void changed();
    /// A friend request, accept or remove finished; @p ok false comes with a message to show.
    void actionFinished(bool ok, const QString& message);

   private:
    using Done = std::function<void(int status, const QJsonObject& body)>;
    void call(const QByteArray& method, const QString& path, const QJsonObject& body, Done done);
    void claim();
    void sendPresence();
    void friendAction(const QByteArray& method, const QString& path, const QString& name, const QString& okMessage);
    void setError(const QString& error);
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
    bool m_claiming = false;
    bool m_watching = false;
    bool m_loaded = false;
    QString m_error;
    int m_generation = 0;  ///< bumped on every player change so late answers for the old one are dropped

    QList<Friend> m_friends;
    QList<Request> m_incoming;
    QList<Request> m_outgoing;

    QTimer m_presenceTimer;
    QTimer m_refreshTimer;
};
