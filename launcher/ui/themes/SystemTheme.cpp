#include "SystemTheme.h"
#include <QApplication>
#include <QStyle>
#include <QStyleFactory>
#include "HintOverrideProxyStyle.h"
#include "ThemeManager.h"

// See https://github.com/XylarJava/XylarJava/issues/490
static const QStringList S_NATIVE_STYLES{ "windows11", "windowsvista", "macos", "system", "windows" };

SystemTheme::SystemTheme(const QString& styleName, const QPalette& defaultPalette, bool isDefaultTheme)
{
    m_themeName = isDefaultTheme ? "system" : styleName;
    m_widgetTheme = styleName;
    // NOTE: SystemTheme is reconstructed on page refresh. We can't accurately determine the system palette here
    // See also S_NATIVE_STYLES comment
    if (S_NATIVE_STYLES.contains(m_themeName)) {
        m_colorPalette = defaultPalette;
    } else {
        auto style = QStyleFactory::create(styleName);
        m_colorPalette = style != nullptr ? style->standardPalette() : defaultPalette;
        delete style;
    }
}

void SystemTheme::apply(bool initial)
{
    // See S_NATIVE_STYLES comment
    if (initial && S_NATIVE_STYLES.contains(m_themeName)) {
        QApplication::setStyle(new HintOverrideProxyStyle(QStyleFactory::create(qtTheme())));
        return;
    }

    ITheme::apply(initial);
}

QString SystemTheme::id()
{
    return m_themeName;
}

QString SystemTheme::name()
{
    if (m_themeName.toLower() == "windowsvista") {
        return QObject::tr("Windows Vista");
    } else if (m_themeName.toLower() == "windows") {
        return QObject::tr("Windows 9x");
    } else if (m_themeName.toLower() == "windows11") {
        return QObject::tr("Windows 11");
    } else if (m_themeName.toLower() == "system") {
        return QObject::tr("System");
    } else {
        return m_themeName;
    }
}

QString SystemTheme::tooltip()
{
    if (m_themeName.toLower() == "windowsvista") {
        return QObject::tr("Widget style trying to look like your win32 theme");
    } else if (m_themeName.toLower() == "windows") {
        return QObject::tr("Windows 9x inspired widget style");
    } else if (m_themeName.toLower() == "windows11") {
        return QObject::tr("WinUI 3 inspired Qt widget style");
    } else if (m_themeName.toLower() == "fusion") {
        return QObject::tr("The default Qt widget style");
    } else if (m_themeName.toLower() == "system") {
        return QObject::tr("Your current system theme");
    } else {
        return "";
    }
}

QString SystemTheme::qtTheme()
{
    return m_widgetTheme;
}

QPalette SystemTheme::colorScheme()
{
    return m_colorPalette;
}

QString SystemTheme::appStyleSheet()
{
    return QString();
}

double SystemTheme::fadeAmount()
{
    return 0.5;
}

QColor SystemTheme::fadeColor()
{
    return QColor(128, 128, 128);
}

bool SystemTheme::hasStyleSheet()
{
    return false;
}
