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

/// Runs the repair and returns what was done, one line per step, for the log.
QStringList repair(BaseInstance* instance);

}  // namespace InstanceRepair
