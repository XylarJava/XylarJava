#include "PlayitTunnel.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSysInfo>
#include <QTimer>

#include <utility>

#include "BuildConfig.h"
#include "PrivateBrowser.h"
#include "java/JavaUtils.h"

namespace {
// Pinned so an agent update can't change the command line under us.
const char* AGENT_RELEASE = "https://github.com/playit-cloud/playit-agent/releases/download/v1.2.0/";
const char* API = "https://api.playit.gg";
const char* DAEMON = "playit";
const char* CLI = "playit-cli";
const char* SECRET_FILE = "secret.txt";
const char* SOCKET_FILE = "playit.sock";
const char* LOG_FILE = "playit.log";
const int REFRESH_SECS = 20;

QString agentArch()
{
    const QString arch = QSysInfo::currentCpuArchitecture();
    if (arch == QLatin1String("x86_64"))
        return QStringLiteral("amd64");
    if (arch == QLatin1String("arm64"))
        return QStringLiteral("aarch64");
    return QString();
}
}  // namespace

PlayitTunnel::PlayitTunnel(const QString& folder, QNetworkAccessManager* network, QObject* parent)
    : QObject(parent), m_folder(folder), m_network(network)
{
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(REFRESH_SECS * 1000);
    connect(m_refreshTimer, &QTimer::timeout, this, &PlayitTunnel::refresh);
    if (!supported())
        m_state = State::Unsupported;
    else if (!secret().isEmpty())
        m_state = State::Ready;
}

PlayitTunnel::~PlayitTunnel()
{
    stopAgent();
}

bool PlayitTunnel::supported()
{
#ifdef Q_OS_LINUX
    return !agentArch().isEmpty();
#else
    return false;
#endif
}

QUrl PlayitTunnel::tunnelsPage()
{
    return QUrl(QStringLiteral("https://playit.gg/account/tunnels"));
}

QString PlayitTunnel::path(const QString& name) const
{
    return QDir(m_folder).filePath(name);
}

QString PlayitTunnel::secret() const
{
    QFile file(path(QString::fromLatin1(SECRET_FILE)));
    if (!file.open(QIODevice::ReadOnly))
        return QString();
    const QString value = QString::fromLatin1(file.readAll()).trimmed();
    static const QRegularExpression valid(QStringLiteral("^[0-9a-f]{32,}$"));
    return valid.match(value).hasMatch() ? value : QString();
}

void PlayitTunnel::setState(State state, const QString& message)
{
    m_state = state;
    m_message = message;
    emit changed();
}

QString PlayitTunnel::addressFor(int localPort) const
{
    for (const Tunnel& tunnel : m_tunnels) {
        if (tunnel.minecraftJava && !tunnel.disabled && tunnel.localPort == localPort)
            return tunnel.address;
    }
    return QString();
}

void PlayitTunnel::download(const QString& asset, const QString& target, std::function<void(bool)> done)
{
    QNetworkRequest request(QUrl(QString::fromLatin1(AGENT_RELEASE) + asset));
    request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, target, done] {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        // The agent is a static ELF binary of several MB: anything else is an error page.
        if (reply->error() != QNetworkReply::NoError || data.size() < 1024 * 1024 || !data.startsWith("\x7f" "ELF")) {
            done(false);
            return;
        }
        QSaveFile file(target);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            done(false);
            return;
        }
        QFile::setPermissions(target, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        done(true);
    });
}

void PlayitTunnel::ensureAgent(std::function<void(bool)> done)
{
    const QString daemon = path(QString::fromLatin1(DAEMON));
    const QString cli = path(QString::fromLatin1(CLI));
    if (QFileInfo(daemon).isExecutable() && QFileInfo(cli).isExecutable()) {
        done(true);
        return;
    }
    QDir().mkpath(m_folder);
    QFile::setPermissions(m_folder, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    const QString arch = agentArch();
    m_message = tr("Downloading the playit.gg agent...");
    emit changed();
    download(QStringLiteral("playit-linux-%1").arg(arch), daemon, [this, arch, cli, done](bool ok) {
        if (!ok)
            return done(false);
        download(QStringLiteral("playit-cli-linux-%1").arg(arch), cli, done);
    });
}

void PlayitTunnel::runCli(const QStringList& args, int timeoutSecs, std::function<void(bool, const QString&)> done)
{
    auto process = new QProcess(this);
    process->setProcessEnvironment(CleanEnviroment());
    auto timer = new QTimer(process);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, [process] { process->kill(); });
    connect(process, &QProcess::finished, this, [process, done](int exitCode, QProcess::ExitStatus status) {
        process->deleteLater();
        done(status == QProcess::NormalExit && exitCode == 0, QString::fromUtf8(process->readAllStandardOutput()));
    });
    connect(process, &QProcess::errorOccurred, this, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            done(false, QString());
        }
    });
    process->start(path(QString::fromLatin1(CLI)), args);
    timer->start(timeoutSecs * 1000);
}

void PlayitTunnel::link()
{
    if (m_state == State::Unsupported || m_state == State::Linking)
        return;
    setState(State::Linking, tr("Getting playit.gg ready..."));
    ensureAgent([this](bool ok) {
        if (!ok)
            return setState(State::NotLinked, tr("Couldn't download the playit.gg agent. Check your connection and try again."));
        runCli({ QStringLiteral("claim"), QStringLiteral("generate") }, 30, [this](bool ok, const QString& output) {
            const QString code = output.trimmed().section('\n', -1).trimmed();
            static const QRegularExpression validCode(QStringLiteral("^[0-9a-f]{6,64}$"));
            if (!ok || !validCode.match(code).hasMatch())
                return setState(State::NotLinked, tr("playit.gg didn't give a link code. Try again in a moment."));
            openPage(QUrl(QStringLiteral("https://playit.gg/claim/%1").arg(code)));
            setState(State::Linking, tr("Sign up or log in to playit.gg in the Firefox window (free account), then approve this computer."));
            // "claim exchange" prints progress lines while it waits, then the secret on the last line.
            runCli({ QStringLiteral("claim"), QStringLiteral("exchange"), code, QStringLiteral("--wait"), QStringLiteral("600") }, 620,
                   [this](bool ok, const QString& output) {
                       const QString value = output.trimmed().section('\n', -1).trimmed();
                       static const QRegularExpression validSecret(QStringLiteral("^[0-9a-f]{32,}$"));
                       if (!ok || !validSecret.match(value).hasMatch())
                           return setState(State::NotLinked, tr("The agent wasn't approved in time. Press the button to try again."));
                       QSaveFile file(path(QString::fromLatin1(SECRET_FILE)));
                       if (!file.open(QIODevice::WriteOnly) || file.write(value.toLatin1()) < 0 || !file.commit())
                           return setState(State::NotLinked, tr("Couldn't save the playit.gg key in %1.").arg(m_folder));
                       QFile::setPermissions(path(QString::fromLatin1(SECRET_FILE)), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
                       setState(State::Ready, QString());
                       if (m_users > 0)
                           startAgent();
                       m_lastRefresh = QDateTime();
                       refresh();
                   });
        });
    });
}

bool PlayitTunnel::agentAlreadyRunning() const
{
    QLocalSocket socket;
    socket.connectToServer(path(QString::fromLatin1(SOCKET_FILE)));
    return socket.waitForConnected(300);
}

void PlayitTunnel::startAgent()
{
    if (m_state != State::Ready || (m_agent && m_agent->state() != QProcess::NotRunning))
        return;
    // An agent started outside the launcher with the same socket already serves our tunnels.
    if (agentAlreadyRunning())
        return;
    ensureAgent([this](bool ok) {
        if (!ok) {
            m_message = tr("Couldn't download the playit.gg agent. Check your connection.");
            emit changed();
            return;
        }
        if (!m_agent) {
            m_agent = new QProcess(this);
            m_agent->setProcessEnvironment(CleanEnviroment());
            m_agent->setProcessChannelMode(QProcess::MergedChannels);
            connect(m_agent, &QProcess::readyRead, m_agent, [this] { m_agent->readAll(); });
            connect(m_agent, &QProcess::finished, this, [this](int exitCode) {
                if (m_users > 0) {
                    m_message = tr("The playit.gg agent stopped (exit code %1). See %2.").arg(exitCode).arg(path(QString::fromLatin1(LOG_FILE)));
                    emit changed();
                }
            });
        }
        QFile::remove(path(QString::fromLatin1(SOCKET_FILE)));
        m_agent->start(path(QString::fromLatin1(DAEMON)),
                       { QStringLiteral("--secret-path"), path(QString::fromLatin1(SECRET_FILE)), QStringLiteral("--socket-path"),
                         path(QString::fromLatin1(SOCKET_FILE)), QStringLiteral("-l"), path(QString::fromLatin1(LOG_FILE)) });
    });
}

void PlayitTunnel::stopAgent()
{
    if (!m_agent || m_agent->state() == QProcess::NotRunning)
        return;
    m_agent->terminate();
    if (!m_agent->waitForFinished(3000))
        m_agent->kill();
}

void PlayitTunnel::acquire()
{
    m_users++;
    startAgent();
    m_refreshTimer->start();
    m_lastRefresh = QDateTime();
    refresh();
}

void PlayitTunnel::release()
{
    m_users = qMax(0, m_users - 1);
    if (m_users > 0)
        return;
    m_refreshTimer->stop();
    stopAgent();
}

void PlayitTunnel::refresh()
{
    if (m_lastRefresh.isValid() && m_lastRefresh.secsTo(QDateTime::currentDateTimeUtc()) < 5)
        return;
    fetch({});
}

void PlayitTunnel::call(const QString& endpoint, const QJsonObject& body, std::function<void(bool ok, const QJsonValue& data)> done)
{
    QNetworkRequest request(QUrl(QString::fromLatin1(API) + endpoint));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
    request.setRawHeader("Authorization", "Agent-Key " + secret().toLatin1());
    QNetworkReply* reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        done(root.value("status").toString() == QLatin1String("success"), root.value("data"));
    });
}

void PlayitTunnel::fetch(std::function<void()> then)
{
    if (then)
        m_afterFetch << then;
    if (secret().isEmpty()) {
        const auto callbacks = std::exchange(m_afterFetch, {});
        for (const auto& callback : callbacks)
            callback();
        return;
    }
    if (m_refreshing)
        return;  // the callbacks run when the request in flight comes back
    m_refreshing = true;
    m_lastRefresh = QDateTime::currentDateTimeUtc();
    call(QStringLiteral("/agents/rundata"), QJsonObject(), [this](bool ok, const QJsonValue& value) {
        m_refreshing = false;
        if (!ok) {
            const QString error = value.toVariant().toString();
            m_message = error.contains(QLatin1String("Invalid"), Qt::CaseInsensitive)
                            ? tr("playit.gg doesn't accept this computer's key any more. Link playit.gg again.")
                            : tr("Couldn't reach playit.gg right now.");
        } else {
            const QJsonObject data = value.toObject();
            m_agentId = data.value("agent_id").toString();
            QList<Tunnel> tunnels;
            for (const QJsonValue& entry : data.value("tunnels").toArray()) {
                const QJsonObject t = entry.toObject();
                Tunnel tunnel;
                tunnel.id = t.value("id").toString();
                const QString custom = t.value("custom_domain").toString();
                tunnel.address = custom.isEmpty() ? t.value("assigned_domain").toString() : custom;
                tunnel.localPort = t.value("local_port").toInt();
                tunnel.minecraftJava = t.value("tunnel_type").toString() == QLatin1String("minecraft-java");
                tunnel.disabled = !t.value("disabled").isNull() && !t.value("disabled").isUndefined();
                tunnels << tunnel;
            }
            m_tunnels = tunnels;
            const QString account = data.value("account_status").toString();
            m_message = account == QLatin1String("email_not_verified") || account == QLatin1String("email-not-verified")
                            ? tr("Verify your email on playit.gg to use tunnels.")
                            : QString();
        }
        emit changed();
        const auto callbacks = std::exchange(m_afterFetch, {});
        for (const auto& callback : callbacks)
            callback();
    });
}

void PlayitTunnel::createTunnel(const QString& name, int localPort, std::function<void(const QString& error)> done)
{
    if (m_state != State::Ready)
        return done(tr("Link playit.gg first."));
    fetch([this, name, localPort, done] {
        if (!addressFor(localPort).isEmpty())
            return done(QString());
        if (m_agentId.isEmpty())
            return done(m_message.isEmpty() ? tr("Couldn't reach playit.gg right now.") : m_message);
        const QJsonObject origin{ { "type", "agent" },
                                  { "data", QJsonObject{ { "agent_id", m_agentId }, { "local_ip", "127.0.0.1" }, { "local_port", localPort } } } };
        const QJsonObject body{ { "name", name.left(60) }, { "tunnel_type", "minecraft-java" }, { "port_type", "tcp" }, { "port_count", 1 },
                                { "origin", origin }, { "enabled", true }, { "alloc", QJsonValue::Null }, { "firewall_id", QJsonValue::Null } };
        call(QStringLiteral("/tunnels/create"), body, [this, localPort, done](bool ok, const QJsonValue& data) {
            if (!ok) {
                const QString reason = data.isObject() ? data.toObject().value("message").toString() : data.toVariant().toString();
                return done(tr("playit.gg didn't make the tunnel (%1). Free accounts can only have a few: delete one you don't use "
                               "on playit.gg and try again.")
                                .arg(reason.isEmpty() ? tr("no reason given") : reason));
            }
            waitForAddress(localPort, 10, done);
        });
    });
}

void PlayitTunnel::waitForAddress(int localPort, int triesLeft, std::function<void(const QString& error)> done)
{
    fetch([this, localPort, triesLeft, done] {
        // The tunnel works either way; its address also shows up later through the regular refresh.
        if (!addressFor(localPort).isEmpty() || triesLeft <= 1)
            return done(QString());
        QTimer::singleShot(2000, this, [this, localPort, triesLeft, done] { waitForAddress(localPort, triesLeft - 1, done); });
    });
}

void PlayitTunnel::deleteTunnel(int localPort)
{
    for (const Tunnel& tunnel : m_tunnels) {
        if (tunnel.minecraftJava && tunnel.localPort == localPort && !tunnel.id.isEmpty()) {
            call(QStringLiteral("/tunnels/delete"), QJsonObject{ { "tunnel_id", tunnel.id } }, [this](bool, const QJsonValue&) { fetch({}); });
            return;
        }
    }
}

void PlayitTunnel::openPage(const QUrl& url)
{
    PrivateBrowser::open(url, QFileInfo(m_folder).absolutePath(), m_network);
}
