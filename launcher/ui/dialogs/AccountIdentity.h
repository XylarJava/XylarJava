#pragma once
#include "minecraft/auth/MinecraftAccount.h"
class QWidget;
namespace AccountIdentity {
/** Verifies ownership and reserves the username. Shows progress and explains a rejection. */
bool verify(QWidget* parent, const MinecraftAccountPtr& account);
}  // namespace AccountIdentity
