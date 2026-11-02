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

#include "BuildConfig.h"
#include "DesktopServices.h"
#include "java/JavaUtils.h"

namespace {
// Pinned so an agent update can't change the command line under us.
const char* AGENT_RELEASE = "https://github.com/playit-cloud/playit-agent/releases/download/v1.2.0/";
const char* API_RUNDATA = "https://api.playit.gg/agents/rundata";
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
            DesktopServices::openUrl(QUrl(QStringLiteral("https://playit.gg/claim/%1").arg(code)));
            setState(State::Linking, tr("Log in to playit.gg in your browser (free account) and approve the agent."));
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
    const QString key = secret();
    if (key.isEmpty() || m_refreshing)
        return;
    if (m_lastRefresh.isValid() && m_lastRefresh.secsTo(QDateTime::currentDateTimeUtc()) < 5)
        return;
    m_refreshing = true;
    m_lastRefresh = QDateTime::currentDateTimeUtc();
    QNetworkRequest request(QUrl(QString::fromLatin1(API_RUNDATA)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setHeader(QNetworkRequest::UserAgentHeader, BuildConfig.USER_AGENT);
    request.setRawHeader("Authorization", "Agent-Key " + key.toLatin1());
    QNetworkReply* reply = m_network->post(request, QByteArrayLiteral("{}"));
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        m_refreshing = false;
        const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
        if (root.value("status").toString() != QLatin1String("success")) {
            const QString error = root.value("data").toVariant().toString();
            m_message = error.contains(QLatin1String("Invalid"), Qt::CaseInsensitive)
                            ? tr("playit.gg doesn't accept this computer's key any more. Link playit.gg again.")
                            : tr("Couldn't reach playit.gg right now.");
            emit changed();
            return;
        }
        const QJsonObject data = root.value("data").toObject();
        QList<Tunnel> tunnels;
        for (const QJsonValue& value : data.value("tunnels").toArray()) {
            const QJsonObject t = value.toObject();
            Tunnel tunnel;
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
        emit changed();
    });
}
