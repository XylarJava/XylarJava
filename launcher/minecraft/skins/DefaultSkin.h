#pragma once

#include <QImage>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QObject;

/**
 * Works out which built-in default skin the game gives a player who has no
 * custom skin, the same way the game does it (from the player's UUID), and
 * loads that texture from a Minecraft client jar the launcher has already
 * downloaded. Nothing is bundled with the launcher: if no client jar is on
 * disk yet, no texture is returned until downloadClientJar() has fetched one.
 */
namespace DefaultSkin {

struct Result {
    QImage texture;  ///< Null when no suitable client jar was found.
    bool slim = false;
    QString name;  ///< e.g. "steve", "alex", "zuri"
};

/// Java's UUID.hashCode() for a UUID string (with or without dashes).
int uuidHashCode(const QString& uuid);

/**
 * Default skin for @p uuid. @p preferredVersion is the Minecraft version whose
 * client jar should be tried first (usually the selected instance's version).
 */
Result forUuid(const QString& uuid, const QString& preferredVersion = QString());

/// True when at least one Minecraft client jar is on disk to read default skins from.
bool haveClientJar();

/**
 * Downloads the Minecraft 1.8.9 client jar from Mojang (checked against its SHA-1) to where the
 * launcher keeps it, so a fresh install shows a real default skin instead of a placeholder. It is
 * the jar the XylarJava Client plays on, so the launcher would download it anyway. @p done runs
 * in @p context with whether it worked; a call while a download is running only waits for it.
 */
void downloadClientJar(QNetworkAccessManager* network, QObject* context, std::function<void(bool ok)> done);

}  // namespace DefaultSkin
