#include "DiscordPresence.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTimer>
#include <QUuid>
#include <QtEndian>

namespace {
// Discord IPC opcodes
constexpr int kOpHandshake = 0;
constexpr int kOpFrame = 1;
constexpr int kOpClose = 2;

constexpr int kMaxPipes = 10;
constexpr int kRetryMsecs = 15000;
}  // namespace

DiscordPresence::DiscordPresence(QObject* parent) : QObject(parent)
{
    m_socket = new QLocalSocket(this);
    connect(m_socket, &QLocalSocket::connected, this, &DiscordPresence::onConnected);
    connect(m_socket, &QLocalSocket::readyRead, this, &DiscordPresence::onReadyRead);
    connect(m_socket, &QLocalSocket::disconnected, this, &DiscordPresence::onDisconnected);
    connect(m_socket, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        if (m_socket->state() == QLocalSocket::ConnectedState)
            return;
        // Try the next pipe; after the last one, wait and start over.
        m_pipeIndex++;
        if (m_pipeIndex < kMaxPipes && isActive()) {
            QTimer::singleShot(0, this, &DiscordPresence::tryConnect);
        } else {
            m_pipeIndex = 0;
            if (isActive())
                m_retryTimer->start();
        }
    });

    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    m_retryTimer->setInterval(kRetryMsecs);
    connect(m_retryTimer, &QTimer::timeout, this, &DiscordPresence::tryConnect);

    showIdle();
}

DiscordPresence::~DiscordPresence()
{
    if (m_socket->state() == QLocalSocket::ConnectedState)
        m_socket->abort();
}

void DiscordPresence::setClientId(const QString& clientId)
{
    const QString trimmed = clientId.trimmed();
    if (trimmed == m_clientId)
        return;
    m_clientId = trimmed;
    restart();
}

void DiscordPresence::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return;
    m_enabled = enabled;
    restart();
}

void DiscordPresence::restart()
{
    m_retryTimer->stop();
    m_ready = false;
    m_buffer.clear();
    if (m_socket->state() != QLocalSocket::UnconnectedState)
        m_socket->abort();
    m_pipeIndex = 0;
    if (isActive())
        tryConnect();
}

QString DiscordPresence::socketName(int index) const
{
    const QString name = QStringLiteral("discord-ipc-%1").arg(index);
#ifdef Q_OS_WIN
    return name;  // QLocalSocket maps this to \\.\pipe\discord-ipc-N
#else
    // Discord puts its socket in the runtime/temp dir; Flatpak and Snap builds use a subfolder.
    QStringList bases;
    for (const char* var : { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" }) {
        const QString value = qEnvironmentVariable(var);
        if (!value.isEmpty())
            bases << value;
    }
    bases << QStringLiteral("/tmp");
    for (const QString& base : bases) {
        for (const QString& sub : { QString(), QStringLiteral("app/com.discordapp.Discord"), QStringLiteral("snap.discord") }) {
            const QString path = QDir(sub.isEmpty() ? base : QDir(base).filePath(sub)).filePath(name);
            if (QFileInfo::exists(path))
                return path;
        }
    }
    return QDir(bases.first()).filePath(name);
#endif
}

void DiscordPresence::tryConnect()
{
    if (!isActive() || m_socket->state() != QLocalSocket::UnconnectedState)
        return;
    m_socket->connectToServer(socketName(m_pipeIndex));
}

void DiscordPresence::onConnected()
{
    m_pipeIndex = 0;
    m_buffer.clear();
    QJsonObject handshake;
    handshake.insert(QStringLiteral("v"), 1);
    handshake.insert(QStringLiteral("client_id"), m_clientId);
    sendFrame(kOpHandshake, handshake);
}

void DiscordPresence::onReadyRead()
{
    m_buffer.append(m_socket->readAll());
    while (m_buffer.size() >= 8) {
        const int opcode = qFromLittleEndian<qint32>(m_buffer.constData());
        const int length = qFromLittleEndian<qint32>(m_buffer.constData() + 4);
        if (length < 0 || length > 1024 * 1024) {
            m_socket->abort();
            return;
        }
        if (m_buffer.size() < 8 + length)
            return;
        const QJsonObject message = QJsonDocument::fromJson(m_buffer.mid(8, length)).object();
        m_buffer.remove(0, 8 + length);

        if (opcode == kOpClose) {
            m_socket->disconnectFromServer();
            return;
        }
        if (opcode == kOpFrame && message.value(QStringLiteral("evt")).toString() == QLatin1String("READY")) {
            m_ready = true;
            pushActivity();
        }
    }
}

void DiscordPresence::onDisconnected()
{
    m_ready = false;
    m_buffer.clear();
    if (isActive())
        m_retryTimer->start();
}

void DiscordPresence::sendFrame(int opcode, const QJsonObject& payload)
{
    const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    QByteArray frame(8, Qt::Uninitialized);
    qToLittleEndian<qint32>(opcode, frame.data());
    qToLittleEndian<qint32>(static_cast<qint32>(body.size()), frame.data() + 4);
    frame.append(body);
    m_socket->write(frame);
    m_socket->flush();
}

void DiscordPresence::showIdle()
{
    QJsonObject assets;
    assets.insert(QStringLiteral("large_image"), QStringLiteral("logo"));
    assets.insert(QStringLiteral("large_text"), QStringLiteral("XylarJava"));

    QJsonObject activity;
    activity.insert(QStringLiteral("details"), tr("In the launcher"));
    activity.insert(QStringLiteral("state"), tr("Picking what to play"));
    activity.insert(QStringLiteral("assets"), assets);
    m_activity = activity;
    pushActivity();
}

void DiscordPresence::showPlaying(const QString& instanceName, const QString& details, qint64 startedAtSecs)
{
    QJsonObject assets;
    assets.insert(QStringLiteral("large_image"), QStringLiteral("logo"));
    assets.insert(QStringLiteral("large_text"), QStringLiteral("XylarJava"));

    QJsonObject timestamps;
    timestamps.insert(QStringLiteral("start"), startedAtSecs);

    QJsonObject activity;
    activity.insert(QStringLiteral("details"), tr("Playing %1").arg(instanceName));
    if (!details.isEmpty())
        activity.insert(QStringLiteral("state"), details);
    activity.insert(QStringLiteral("timestamps"), timestamps);
    activity.insert(QStringLiteral("assets"), assets);
    m_activity = activity;
    pushActivity();
}

void DiscordPresence::pushActivity()
{
    if (!m_ready || m_socket->state() != QLocalSocket::ConnectedState)
        return;

    QJsonObject args;
    args.insert(QStringLiteral("pid"), static_cast<qint64>(QCoreApplication::applicationPid()));
    args.insert(QStringLiteral("activity"), m_activity);

    QJsonObject command;
    command.insert(QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY"));
    command.insert(QStringLiteral("args"), args);
    command.insert(QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    sendFrame(kOpFrame, command);
}
