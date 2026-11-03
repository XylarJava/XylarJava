#pragma once

#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>

#include "MessageLevel.h"

class BaseInstance;
class LaunchTask;

/**
 * What the running game is doing, read from its log: in the menus, in a singleplayer world or on
 * a server (and which one). Social and Discord show it; friends use the server to join.
 */
class GameActivity : public QObject {
    Q_OBJECT

   public:
    enum class Where { Menus, Singleplayer, Server };

    explicit GameActivity(QObject* parent = nullptr);

    /// Follows @p instance from now on (nullptr: nothing is running).
    void watch(BaseInstance* instance);

    BaseInstance* instance() const { return m_instance; }
    /// When the game started, in seconds since the epoch (0 when nothing runs).
    qint64 startedAt() const { return m_startedAt; }
    Where where() const { return m_where; }
    /// The server's host as the player typed it ("play.example.com", "127.0.0.1"); empty unless on a server.
    QString host() const { return m_host; }
    int port() const { return m_port; }
    /// "host" on the default port 25565, "host:port" otherwise.
    QString address() const;
    /// The Minecraft version the instance plays, e.g. "1.21.4".
    QString minecraftVersion() const;

    /// True for hosts that only make sense on this computer or home network (localhost, 192.168.x.x...).
    static bool isLocalHost(const QString& host);
    /// True when @p host is an IP address rather than a name.
    static bool isIpAddress(const QString& host);

   signals:
    void changed();

   private:
    void followTask(LaunchTask* task);
    void onLine(MessageLevel level, const QString& line);
    void setWhere(Where where, const QString& host = QString(), int port = 25565);

    QPointer<BaseInstance> m_instance;
    QList<QMetaObject::Connection> m_connections;
    QMetaObject::Connection m_logConnection;
    qint64 m_startedAt = 0;
    Where m_where = Where::Menus;
    QString m_host;
    int m_port = 25565;
};
