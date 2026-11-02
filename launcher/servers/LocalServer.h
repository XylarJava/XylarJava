#pragma once

#include <QDateTime>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

#include "ServerSoftware.h"

class QNetworkAccessManager;
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
    bool onlineMode = true;
    bool pvp = true;
    bool openPort = true;  // UPnP
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
    QString lanAddress() const;
    QString publicAddress() const { return m_publicAddress; }
    QString networkMessage() const { return m_networkMessage; }
    bool portOpen() const { return m_portOpen; }
    /// Folder for mods (Fabric) or plugins (Paper); empty for Vanilla.
    QString addonsPath() const;

    void install();
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

   private:
    void setState(State state);
    void appendConsole(const QString& line);
    void writeServerFiles();
    void onOutput();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void openPort();

    QString m_folder;
    QNetworkAccessManager* m_network;
    LocalServerConfig m_config;
    State m_state = State::NotInstalled;
    QProcess* m_process = nullptr;
    QStringList m_console;
    QByteArray m_partialLine;
    UpnpPortMapper* m_upnp = nullptr;
    QString m_publicAddress;
    QString m_networkMessage;
    bool m_portOpen = false;
};
