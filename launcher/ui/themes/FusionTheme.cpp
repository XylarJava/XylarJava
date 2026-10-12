#include "FusionTheme.h"

#include <QObject>
#include <QStyle>
#include <QStyleFactory>

QString FusionTheme::id()
{
    return "fusion";
}

QString FusionTheme::name()
{
    return QObject::tr("Fusion");
}

QString FusionTheme::tooltip()
{
    return QObject::tr("The clean, cross-platform Fusion look.");
}

bool FusionTheme::hasStyleSheet()
{
    return false;
}

QString FusionTheme::appStyleSheet()
{
    return QString();
}

QString FusionTheme::qtTheme()
{
    return "Fusion";
}

QPalette FusionTheme::colorScheme()
{
    // Use the canonical palette that the Fusion style ships with, so the theme
    // looks exactly like stock Fusion regardless of the OS default palette.
    if (QStyle* style = QStyleFactory::create("Fusion")) {
        QPalette palette = style->standardPalette();
        delete style;
        return palette;
    }
    return QPalette();
}

double FusionTheme::fadeAmount()
{
    return 0.5;
}

QColor FusionTheme::fadeColor()
{
    return QColor(128, 128, 128);
}
