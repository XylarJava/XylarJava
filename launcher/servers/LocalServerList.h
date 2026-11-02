#pragma once

#include <QList>
#include <QObject>
#include <QString>

#include "LocalServer.h"

class QNetworkAccessManager;
class PlayitTunnel;

/// All servers made in XylarJava, stored in "<data folder>/servers/<id>/".
class LocalServerList : public QObject {
    Q_OBJECT
   public:
    /// How many servers XylarJava hosts at most.
    static constexpr int MAX_SERVERS = 5;

    LocalServerList(const QString& root, QNetworkAccessManager* network, QObject* parent = nullptr);

    QList<LocalServer*> servers() const { return m_servers; }
    /// Makes a new server; nullptr when there are MAX_SERVERS already.
    LocalServer* create(const LocalServerConfig& config);
    bool isFull() const { return m_servers.size() >= MAX_SERVERS; }
    /// The first port from 25565 up that no other server uses.
    int freePort() const;
    /// A name no other server has: "My server", "My server 2"...
    QString freeName(const QString& base) const;
    /// Deletes the server, its folder (worlds included) and its playit.gg tunnel. Refuses while it's running.
    bool remove(LocalServer* server);
    bool anyRunning() const;
    void stopAll();
    QString root() const { return m_root; }
    /// The playit.gg tunnel shared by all servers ("<data folder>/playit/").
    PlayitTunnel* playit() const { return m_playit; }

   signals:
    void listChanged();

   private:
    QString m_root;
    QNetworkAccessManager* m_network;
    QList<LocalServer*> m_servers;
    PlayitTunnel* m_playit = nullptr;
};
