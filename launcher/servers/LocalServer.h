#pragma once

#include <QDateTime>
#include <QObject>
#include <functional>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "tasks/Task.h"
#include "ServerSoftware.h"

class QNetworkAccessManager;
class PlayitTunnel;
class UpnpPortMapper;

/// What a server is set up with. Saved as xylarjava-server.json in the server's folder.
struct LocalServerConfig {
    QString name;
    ServerSoftware::Kind software = ServerSoftware::Kind::Vanilla;
    QString minecraftVersion;
    QString javaPath;  // empty: pick automatically
    int memoryMb = 2048;
    int port = 25565;
    int maxPlayers = 10;
    QString gamemode = QStringLiteral("survival");
    QString difficulty = QStringLiteral("normal");
    QString motd;
    // Offline mode lets in both Microsoft and offline accounts; the whitelist keeps strangers out.
    bool onlineMode = false;
    bool whitelist = true;  // only the players in whitelist.json can join
    bool pvp = true;
    bool openPort = true;  // UPnP
    bool playit = false;   // friends join through a playit.gg tunnel: IP hidden, attacks filtered
    bool eulaAccepted = false;
};

/**
 * A Minecraft server that runs on this computer: download, start, stop, console, addresses.
 */
class LocalServer : public QObject {
    Q_OBJECT
   public:
    enum class State { NotInstalled, Installing, Stopped, Starting, Running, Stopping };

    LocalServer(const QString& folder, QNetworkAccessManager* network, QObject* parent = nullptr);
    ~LocalServer() override;

    QString id() const;
    QString folder() const { return m_folder; }
    const LocalServerConfig& config() const { return m_config; }
    /// Saves the new configuration; a different software or version means downloading again.
    void setConfig(const LocalServerConfig& config);

    State state() const { return m_state; }
    bool isRunning() const { return m_state == State::Starting || m_state == State::Running || m_state == State::Stopping; }
    QString statusText() const;
    QStringList console() const { return m_console; }
    int port() const { return m_config.port; }
    /// This computer's address on the home network, e.g. "192.168.1.11".
    QString lanIp() const;
    /// The public address once the router (or ipify) told us, e.g. "151.75.128.107"; empty before.
    QString publicIp() const { return m_publicIp; }
    /// What friends type in Multiplayer > Add Server: the IP alone on the default port 25565, "ip:port" otherwise.
    QString joinAddress(const QString& ip) const;
    /// The playit.gg address of this server when it uses playit and the tunnel exists, else empty.
    QString playitAddress() const;
    void setPlayit(PlayitTunnel* playit);
    QString networkMessage() const { return m_networkMessage; }
    bool portOpen() const { return m_portOpen; }
    /// Names of the players on the whitelist.
    QStringList allowedPlayers() const;
    /// Puts a player on the whitelist (works while the server runs too). done gets an empty string on success.
    void allowPlayer(const QString& name, std::function<void(const QString& error)> done);
    /// Takes a player off the whitelist.
    void disallowPlayer(const QString& name);
    /// The UUID an offline-mode server gives this name ("OfflinePlayer:<name>", like the game).
    static QString offlineUuid(const QString& name);
    static bool validPlayerName(const QString& name);
    /// Names the launcher knows with their real capitals (its accounts, Social friends). An offline
    /// server learns them before it starts, so "op" and "whitelist add" give the right UUID.
    static void setKnownPlayers(const QStringList& names);
    /// The name as the player really writes it when the launcher knows it, else as typed.
    static QString knownSpelling(const QString& name);
    /// Folder for mods (Fabric) or plugins (Paper); empty for Vanilla.
    QString addonsPath() const;

    void install();
    /// Starts the server; downloads it first (then starts) when it isn't installed yet.
    void start();
    void stop();
    void kill();
    void sendCommand(const QString& command);

    bool load();
    bool save() const;

    /// Picks a Java for a Minecraft version: the configured one, the launcher's, or one found on the system.
    static QString findJava(const QString& configured, int requiredMajor, QString* why);
    /// Major Java version of an executable, or 0 when it doesn't run.
    static int javaMajor(const QString& javaPath);

   signals:
    void stateChanged();
    void consoleLine(const QString& line);
    void installProgress(qint64 done, qint64 total);
    void addressesChanged();
    void failed(const QString& message);
    /// Someone was put on or taken off the whitelist from the console.
    void whitelistChanged();

   private:
    void setState(State state);
    void appendConsole(const QString& line);
    void writeServerFiles();
    void onOutput();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void openPort();
    /// Downloads the Java this Minecraft version needs into the launcher's Java folder, then starts.
    void downloadJava(int major);

    QString m_folder;
    QNetworkAccessManager* m_network;
    LocalServerConfig m_config;
    State m_state = State::NotInstalled;
    QProcess* m_process = nullptr;
    QStringList m_console;
    QByteArray m_partialLine;
    UpnpPortMapper* m_upnp = nullptr;
    PlayitTunnel* m_playit = nullptr;
    bool m_usingPlayit = false;
    QString m_publicIp;
    QString m_networkMessage;
    bool m_portOpen = false;
    bool m_startWhenInstalled = false;
    Task::Ptr m_javaTask;
};
