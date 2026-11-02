#pragma once

#include <QString>
#include <QStringList>

class BaseInstance;

/**
 * Fixes the usual reasons an instance won't start, without touching worlds, mods or settings:
 * damaged libraries or game jar, stale natives, broken version metadata and a Java path that
 * no longer exists. The next launch downloads whatever was removed.
 */
namespace InstanceRepair {

/// True when the game log shows it couldn't open its window / OpenGL context.
bool looksLikeGraphicsFailure(const QString& gameLog);

/// Runs the repair and returns what was done, one line per step, for the log. With the log of the
/// failed launch, graphics tweaks that keep OpenGL from starting (Zink, forced GPU) are undone too.
QStringList repair(BaseInstance* instance, const QString& gameLog = QString());

}  // namespace InstanceRepair
