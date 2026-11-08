#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

#include <functional>

class BaseInstance;

/**
 * Finds out why an instance didn't start or crashed, from its log, and fixes it without touching
 * worlds or settings that aren't part of the problem:
 *
 * - mods: missing ones are downloaded from Modrinth, ones made for another Minecraft version are
 *   swapped for the right build, broken, duplicate or crashing ones are disabled (renamed to
 *   .disabled, so they can be turned back on);
 * - Java too old for the game or a mod, out of memory, a JVM option Java doesn't know;
 * - OpenGL that won't start because of a graphics tweak (Zink, forced GPU, system GLFW);
 * - damaged libraries, game jar, natives or version metadata (downloaded again on the next launch).
 */
namespace InstanceRepair {

struct Diagnosis {
    /// What went wrong, in one or two plain sentences; empty when nothing specific was found.
    QString cause;
    /// What the repair will do, one line each.
    QStringList plan;
    /// Things the repair can't do for the player, one line each.
    QStringList advice;
    QStringList evidence;   ///< relevant, redacted lines from the failing run
    bool blocking = false;  ///< requires user action; retrying or deleting game files cannot fix it
    bool verifyFiles = false;
    QStringList filesToRedownload;  ///< only explicitly identified damaged files

    bool graphics = false;
    bool files = false;  ///< download libraries, game jar, natives and metadata again
    int javaMajor = 0;   ///< Java this needs at least, 0 if Java isn't the problem
    int memoryMiB = 0;   ///< new maximum memory, 0 to leave it
    QString badJvmOption;
    bool resetJvmArgs = false;

    QStringList modsToInstall;                  ///< mod ids to download from Modrinth
    QStringList modsToUpdate;                   ///< mod ids to swap for this Minecraft version's build
    QStringList modsToDisable;                  ///< mod files
    QList<QPair<QString, QString>> requiredBy;  ///< (missing mod id, mod that needs it), disabled if it can't be found
    QHash<QString, QString> modFiles;           ///< mod id -> file in the mods folder
    QHash<QString, QString> modNames;           ///< mod id -> name

    /// True when the log showed a cause the repair knows about.
    bool specific() const { return !cause.isEmpty(); }
    /// Something to tell apart two diagnoses of the same instance.
    QString signature() const { return cause + plan.join(QLatin1Char('|')); }
};

/// True when the game log shows it couldn't open its window / OpenGL context.
bool looksLikeGraphicsFailure(const QString& gameLog);

/// Diagnoses external failures without touching the instance or scanning mods.
Diagnosis diagnoseLog(const QString& gameLog);

/// The instance's latest game log and newest crash report, for a repair asked for by hand.
QString lastGameLog(BaseInstance* instance);

/// Reads @p gameLog (and the instance's mods folder) and decides what's wrong.
Diagnosis diagnose(BaseInstance* instance, const QString& gameLog);

/// Does what @p diagnosis planned. @p finished gets what was done and what couldn't be, one line each;
/// it's called later when mods are downloaded, right away otherwise.
void repair(BaseInstance* instance, const Diagnosis& diagnosis, std::function<void(const QStringList& done, const QStringList& failed)> finished);

}  // namespace InstanceRepair
