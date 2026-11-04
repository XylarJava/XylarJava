#pragma once

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class SocialClient;

/**
 * Player profiles from XylarJava Profiles: time played, last and favourite Minecraft version,
 * favourite server. The time is counted from the Social presence; the first time a player is
 * signed in on this install, the time the instances here already have is sent once as a start.
 *
 * A profile can be read by the player and by their friends.
 */
class ProfileClient : public QObject {
    Q_OBJECT

   public:
    struct Profile {
        QString uuid;
        QString name;
        qint64 memberSince = 0;   ///< seconds since the epoch
        qint64 playtime = 0;      ///< seconds
        qint64 lastPlayedAt = 0;  ///< seconds since the epoch, 0 when never
        QString lastVersion;
        QString favoriteVersion;
        QString favoriteServer;
        QList<QPair<QString, qint64>> versions;  ///< most played first, with seconds
        QList<QPair<QString, qint64>> servers;
    };
    using Done = std::function<void(bool ok, const Profile& profile, const QString& error)>;

    ProfileClient(QNetworkAccessManager* network, SocialClient* social, QObject* parent = nullptr);

    void setBackendUrl(const QString& url);

    /// Reads @p name's profile; @p done is called on @p context unless it's gone by then.
    void fetch(const QString& name, QObject* context, Done done);

   private:
    void sendBaselineIfNeeded();

    QNetworkAccessManager* m_network = nullptr;
    SocialClient* m_social = nullptr;
    QString m_backendUrl;
    bool m_sendingBaseline = false;
};
