#pragma once
#include "minecraft/auth/MinecraftAccount.h"
class QWidget;
namespace ElyAccountDialog {
MinecraftAccountPtr login(QWidget* parent);
void showSkin(QWidget* parent, const MinecraftAccountPtr& account);
}  // namespace ElyAccountDialog
