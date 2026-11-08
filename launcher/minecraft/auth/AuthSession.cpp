#include "AuthSession.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include "MinecraftAccount.h"

QString AuthSession::serializeUserProperties()
{
    QJsonObject userAttrs;
    /*
    for (auto key : u.properties.keys())
    {
        auto array = QJsonArray::fromStringList(u.properties.values(key));
        userAttrs.insert(key, array);
    }
    */
    QJsonDocument value(userAttrs);
    return value.toJson(QJsonDocument::Compact);
}

bool AuthSession::MakeOffline(QString offline_playername)
{
    session = "-";
    access_token = "0";
    player_name = offline_playername;
    uuid = MinecraftAccount::uuidFromUsername(player_name).toString(QUuid::Id128);
    user_type = QStringLiteral("offline");
    launchMode = LaunchMode::Offline;
    return true;
}

void AuthSession::MakeDemo(QString name, QString u)
{
    uuid = u;
    session = "-";
    access_token = "0";
    player_name = name;
    user_type = QStringLiteral("offline");
    launchMode = LaunchMode::Demo;
};
