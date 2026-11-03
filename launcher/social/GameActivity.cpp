#include "GameActivity.h"

#include <QDateTime>
#include <QHostAddress>
#include <QRegularExpression>

#include "BaseInstance.h"
#include "launch/LaunchTask.h"
#include "launch/LogModel.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

GameActivity::GameActivity(QObject* parent) : QObject(parent) {}

void GameActivity::watch(BaseInstance* instance)
{
    if (instance == m_instance && (instance == nullptr || m_startedAt != 0))
        return;
    for (const auto& connection : m_connections)
        disconnect(connection);
    m_connections.clear();
    disconnect(m_logConnection);

    m_instance = instance;
    m_startedAt = instance ? QDateTime::currentSecsSinceEpoch() : 0;
    m_where = Where::Menus;
    m_host.clear();
    m_port = 25565;
    if (instance) {
        m_connections << connect(instance, &BaseInstance::launchTaskChanged, this, &GameActivity::followTask);
        followTask(instance->getLaunchTask());
    }
    emit changed();
}

void GameActivity::followTask(LaunchTask* task)
{
    disconnect(m_logConnection);
    if (!task)
        return;
    if (auto model = task->getLogModel())
        m_logConnection = connect(model.get(), &LogModel::lineAdded, this, &GameActivity::onLine);
}

void GameActivity::onLine(MessageLevel, const QString& line)
{
    // The lines the game writes as it moves around (the same in every version since 1.7).
    static const QRegularExpression connecting(QStringLiteral("Connecting to ([^,\\s]+), ([0-9]{1,5})"));
    const auto match = connecting.match(line);
    if (match.hasMatch()) {
        QString host = match.captured(1);
        while (host.endsWith(QLatin1Char('.')))
            host.chop(1);
        setWhere(Where::Server, host, match.captured(2).toInt());
        return;
    }
    if (line.contains(QLatin1String("Starting integrated minecraft server")) || line.contains(QLatin1String("Preparing level"))) {
        setWhere(Where::Singleplayer);
        return;
    }
    if (line.contains(QLatin1String("Client disconnected with reason")) || line.contains(QLatin1String("Stopping server")) ||
        line.contains(QLatin1String("Disconnected from server")) || line.contains(QLatin1String("Couldn't connect to server"))) {
        // "Stopping server" is the singleplayer world closing; a server we were on says nothing like it.
        if (m_where != Where::Menus)
            setWhere(Where::Menus);
    }
}

void GameActivity::setWhere(Where where, const QString& host, int port)
{
    if (where == m_where && host == m_host && port == m_port)
        return;
    m_where = where;
    m_host = host;
    m_port = port;
    emit changed();
}

QString GameActivity::address() const
{
    if (m_host.isEmpty())
        return {};
    return m_port == 25565 ? m_host : QStringLiteral("%1:%2").arg(m_host).arg(m_port);
}

QString GameActivity::minecraftVersion() const
{
    if (auto minecraft = dynamic_cast<MinecraftInstance*>(m_instance.data())) {
        if (auto profile = minecraft->getPackProfile())
            return profile->getComponentVersion(QStringLiteral("net.minecraft"));
    }
    return {};
}

bool GameActivity::isIpAddress(const QString& host)
{
    QHostAddress address;
    return address.setAddress(host);
}

bool GameActivity::isLocalHost(const QString& host)
{
    if (host.isEmpty() || host.compare(QLatin1String("localhost"), Qt::CaseInsensitive) == 0 ||
        host.endsWith(QLatin1String(".local"), Qt::CaseInsensitive) || host.endsWith(QLatin1String(".lan"), Qt::CaseInsensitive))
        return true;
    QHostAddress address;
    if (!address.setAddress(host))
        return false;
    return address.isLoopback() || address.isPrivateUse() || address.isLinkLocal() || address == QHostAddress::Any ||
           address == QHostAddress::AnyIPv6;
}
