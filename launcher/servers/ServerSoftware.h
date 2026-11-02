#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;

/**
 * Where server jars come from: Mojang (Vanilla), Fabric's meta server and PaperMC's download service.
 * Everything is asynchronous; callbacks run on the GUI thread.
 */
namespace ServerSoftware {

enum class Kind { Vanilla, Fabric, Paper };

QString id(Kind kind);
Kind fromId(const QString& id);
QString displayName(Kind kind);
/// Folder for mods or plugins inside the server, empty when the software has none.
QString addonsFolder(Kind kind);

/// Java major version a Minecraft version needs.
int requiredJava(const QString& minecraftVersion);

using VersionsCallback = std::function<void(const QStringList& versions, const QString& error)>;
using UrlCallback = std::function<void(const QUrl& url, const QString& error)>;

/// Release versions, newest first.
void fetchVersions(QNetworkAccessManager* network, Kind kind, QObject* context, VersionsCallback done);

/// The URL of a runnable server jar for this software and Minecraft version.
void resolveJar(QNetworkAccessManager* network, Kind kind, const QString& minecraftVersion, QObject* context, UrlCallback done);

}  // namespace ServerSoftware
