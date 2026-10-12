#pragma once

#include "ITheme.h"

/**
 * The plain Qt "Fusion" look: a clean, consistent cross-platform style with
 * Qt's standard Fusion palette. Also serves as the base class for the themed
 * variants (DarkTheme, BrightTheme, …) which override the palette and ids.
 */
class FusionTheme : public ITheme {
   public:
    virtual ~FusionTheme() {}

    QString id() override;
    QString name() override;
    QString tooltip() override;
    bool hasStyleSheet() override;
    QString appStyleSheet() override;
    QString qtTheme() override;
    QPalette colorScheme() override;
    QColor fadeColor() override;
    double fadeAmount() override;
};
