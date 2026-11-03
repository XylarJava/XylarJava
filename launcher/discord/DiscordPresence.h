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
 * needed. Nothing happens until the feature is enabled; if Discord isn't
 * running it quietly retries in the background.
 */
class DiscordPresence : public QObject {
    Q_OBJECT

   public:
    explicit DiscordPresence(QObject* parent = nullptr);
    ~DiscordPresence() override;

    /// XylarJava's own Discord application, used when no other client ID is set.
    static QString defaultClientId();

    /// Discord application ID from discord.com/developers; empty uses XylarJava's.
    void setClientId(const QString& clientId);
    void setEnabled(bool enabled);
    bool isActive() const { return m_enabled && !m_clientId.isEmpty(); }

    /// A button under the status (Discord shows it to everyone else); an empty URL removes it.
    void setButton(const QString& label, const QString& url);

    /// What Discord shows: two lines and the time elapsed since @p startedAtSecs (seconds since the epoch).
    void show(const QString& details, const QString& state, qint64 startedAtSecs);

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
    QTimer* m_pushTimer = nullptr;
    QString m_clientId;
    bool m_enabled = true;
    bool m_ready = false;
    int m_pipeIndex = 0;
    QByteArray m_buffer;
    QString m_buttonLabel;
    QString m_buttonUrl;
    QJsonObject m_activity;
    QJsonObject m_sentActivity;
};
