#pragma once

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>
#include <QUrl>

#include <functional>

class QNetworkAccessManager;
class QProcess;
class QTimer;

/**
 * playit.gg tunnel for servers hosted on this computer: friends join through playit's network,
 * which filters attacks, and never see this computer's IP. No router setup needed.
 *
 * Every user links their own (free) playit.gg account once; the agent's secret stays in
 * "<data folder>/playit/" on this computer. The agent itself (playit-cli and the playit daemon)
 * is downloaded from playit's GitHub releases the first time.
 */
class PlayitTunnel : public QObject {
    Q_OBJECT
   public:
    enum class State { Unsupported, NotLinked, Linking, Ready };

    PlayitTunnel(const QString& folder, QNetworkAccessManager* network, QObject* parent = nullptr);
    ~PlayitTunnel() override;

    /// Whether playit's agent exists for this system (Linux x86_64 / arm64 for now).
    static bool supported();
    /// Where tunnels are made and managed on playit.gg.
    static QUrl tunnelsPage();

    State state() const { return m_state; }
    /// What's going on, for the user; empty when there's nothing to say.
    QString message() const { return m_message; }
    /// What friends type for a server on this local port, empty when there's no tunnel for it yet.
    /// Minecraft Java tunnels have an SRV record, so the name alone is enough.
    QString addressFor(int localPort) const;

    /// Downloads the agent if needed, opens playit.gg to approve it and waits for that.
    void link();
    /// A server that uses playit started: keep the agent running.
    void acquire();
    /// That server stopped: stop the agent when no server needs it any more.
    void release();
    /// Asks playit.gg for this agent's tunnels (at most every few seconds).
    void refresh();

   signals:
    void changed();

   private:
    struct Tunnel {
        QString address;
        int localPort = 0;
        bool minecraftJava = false;
        bool disabled = false;
    };

    QString path(const QString& name) const;
    QString secret() const;
    void setState(State state, const QString& message);
    void ensureAgent(std::function<void(bool)> done);
    void download(const QString& asset, const QString& target, std::function<void(bool)> done);
    void runCli(const QStringList& args, int timeoutSecs, std::function<void(bool ok, const QString& output)> done);
    void startAgent();
    void stopAgent();
    bool agentAlreadyRunning() const;

    QString m_folder;
    QNetworkAccessManager* m_network;
    State m_state = State::NotLinked;
    QString m_message;
    QList<Tunnel> m_tunnels;
    QProcess* m_agent = nullptr;
    QTimer* m_refreshTimer = nullptr;
    QDateTime m_lastRefresh;
    bool m_refreshing = false;
    int m_users = 0;
};
