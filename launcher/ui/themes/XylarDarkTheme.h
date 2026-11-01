#pragma once

#include "FusionTheme.h"

/**
 * XylarJava's default look: a deep, low-contrast dark palette with a warm
 * accent, rounded controls and generous spacing. Built on Fusion so it renders
 * the same on every platform.
 */
class XylarDarkTheme : public FusionTheme {
   public:
    virtual ~XylarDarkTheme() {}

    QString id() override;
    QString name() override;
    QString tooltip() override;
    bool hasStyleSheet() override;
    QString appStyleSheet() override;
    QPalette colorScheme() override;
    double fadeAmount() override;
    QColor fadeColor() override;
};
