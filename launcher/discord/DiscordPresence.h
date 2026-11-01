#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QLocalSocket;
class QTimer;

/**
 * Minimal Discord Rich Presence client speaking Discord's local IPC protocol
 * (a local socket on Linux/macOS, a named pipe on Windows). No Discord SDK is
 * needed. Nothing happens until a client ID is set and the feature is enabled;
 * if Discord isn't running it quietly retries in the background.
 */
class DiscordPresence : public QObject {
    Q_OBJECT

   public:
    explicit DiscordPresence(QObject* parent = nullptr);
    ~DiscordPresence() override;

    /// Discord application ID from discord.com/developers. Empty disables presence.
    void setClientId(const QString& clientId);
    void setEnabled(bool enabled);
    bool isActive() const { return m_enabled && !m_clientId.isEmpty(); }

    /// Shown while the launcher is open and nothing is running.
    void showIdle();
    /// Shown while an instance is running.
    void showPlaying(const QString& instanceName, const QString& details, qint64 startedAtSecs);

   private:
    void restart();
    void tryConnect();
    void onConnected();
    void onReadyRead();
    void onDisconnected();
    void sendFrame(int opcode, const QJsonObject& payload);
    void pushActivity();
    QString socketName(int index) const;

    QLocalSocket* m_socket = nullptr;
    QTimer* m_retryTimer = nullptr;
    QString m_clientId;
    bool m_enabled = true;
    bool m_ready = false;
    int m_pipeIndex = 0;
    QByteArray m_buffer;
    QJsonObject m_activity;
};
