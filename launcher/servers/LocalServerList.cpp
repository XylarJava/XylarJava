#include "LocalServerList.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include "FileSystem.h"
#include "PlayitTunnel.h"

LocalServerList::LocalServerList(const QString& root, QNetworkAccessManager* network, QObject* parent)
    : QObject(parent), m_root(root), m_network(network)
{
    QDir().mkpath(m_root);
    m_playit = new PlayitTunnel(QFileInfo(m_root).dir().filePath(QStringLiteral("playit")), m_network, this);
    const QStringList folders = QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& name : folders) {
        auto server = new LocalServer(QDir(m_root).filePath(name), m_network, this);
        if (server->load()) {
            server->setPlayit(m_playit);
            m_servers << server;
        } else {
            delete server;
        }
    }
}

LocalServer* LocalServerList::create(const LocalServerConfig& config)
{
    QString slug = config.name.toLower();
    slug.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    slug = slug.mid(0, 40);
    while (slug.startsWith('-'))
        slug.remove(0, 1);
    while (slug.endsWith('-'))
        slug.chop(1);
    if (slug.isEmpty())
        slug = QStringLiteral("server");
    QString id = slug;
    for (int i = 2; QDir(m_root).exists(id); i++)
        id = QStringLiteral("%1-%2").arg(slug).arg(i);

    auto server = new LocalServer(QDir(m_root).filePath(id), m_network, this);
    server->setPlayit(m_playit);
    server->setConfig(config);
    m_servers << server;
    emit listChanged();
    return server;
}

bool LocalServerList::remove(LocalServer* server)
{
    if (!server || server->isRunning() || !m_servers.contains(server))
        return false;
    const QString folder = server->folder();
    m_servers.removeAll(server);
    server->deleteLater();
    FS::deletePath(folder);
    emit listChanged();
    return true;
}

bool LocalServerList::anyRunning() const
{
    for (LocalServer* server : m_servers) {
        if (server->isRunning())
            return true;
    }
    return false;
}

void LocalServerList::stopAll()
{
    for (LocalServer* server : m_servers)
        server->stop();
}
