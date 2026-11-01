#pragma once

#include <QImage>
#include <QString>

/**
 * Works out which built-in default skin the game gives a player who has no
 * custom skin, the same way the game does it (from the player's UUID), and
 * loads that texture from a Minecraft client jar the launcher has already
 * downloaded. Nothing is bundled with the launcher: if no client jar is on
 * disk yet, no texture is returned.
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

}  // namespace DefaultSkin
