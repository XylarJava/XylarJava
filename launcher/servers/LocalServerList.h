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
    LocalServerList(const QString& root, QNetworkAccessManager* network, QObject* parent = nullptr);

    QList<LocalServer*> servers() const { return m_servers; }
    LocalServer* create(const LocalServerConfig& config);
    /// Deletes the server and its folder (worlds included). Refuses while it's running.
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
