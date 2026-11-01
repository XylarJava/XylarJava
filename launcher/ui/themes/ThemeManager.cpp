#include "ThemeManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QStyle>
#include <QStyleFactory>

#include "Exception.h"
#include "FileSystem.h"
#include "ui/themes/BrightTheme.h"
#include "ui/themes/CustomTheme.h"
#include "ui/themes/DarkTheme.h"
#include "ui/themes/FusionTheme.h"
#include "ui/themes/SystemTheme.h"
#include "ui/themes/Win7AeroTheme.h"
#include "ui/themes/XylarDarkTheme.h"

#include "Application.h"
#include "settings/SettingsObject.h"

namespace {
// Themes that used to ship with the launcher and were removed. A copy may still
// sit in the user's themes folder from an older install, so skip them by id.
bool isRetiredTheme(const QString& id)
{
    static const QStringList retired{ QStringLiteral("Cat"), QStringLiteral("Tommy-Simpson-SNUPPOVERSE") };
    for (const QString& r : retired) {
        if (id.compare(r, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}
}  // namespace

ThemeManager::ThemeManager()
{
    QIcon::setFallbackThemeName(QIcon::themeName());
    QIcon::setThemeSearchPaths(QIcon::themeSearchPaths() << m_iconThemeFolder.path());

    themeDebugLog() << "Determining System Widget Theme...";
    const auto& style = QApplication::style();
    m_defaultStyle = style->objectName();
    themeDebugLog() << "System theme seems to be:" << m_defaultStyle;

    m_defaultPalette = QApplication::palette();

    initializeThemes();
}

ThemeManager::~ThemeManager()
{
    stopSettingNewWindowColorsOnMac();
}

QString ThemeManager::addTheme(std::unique_ptr<ITheme> theme)
{
    QString id = theme->id();
    if (m_themes.find(id) == m_themes.end())
        m_themes.emplace(id, std::move(theme));
    else
        themeWarningLog() << "Theme(" << id << ") not added to prevent id duplication";
    return id;
}

ITheme* ThemeManager::getTheme(QString themeId)
{
    return m_themes[themeId].get();
}

QString ThemeManager::addIconTheme(IconTheme theme)
{
    QString id = theme.id();
    if (m_icons.find(id) == m_icons.end())
        m_icons.emplace(id, std::move(theme));
    else
        themeWarningLog() << "IconTheme(" << id << ") not added to prevent id duplication";
    return id;
}

void ThemeManager::initializeThemes()
{
    initializeIcons();
    initializeWidgets();
}

void ThemeManager::initializeIcons()
{
    themeDebugLog() << "<> Initializing Icon Themes";

    for (const QString& id : builtinIcons) {
        IconTheme theme(id, QString(":/icons/%1").arg(id));
        if (!theme.load()) {
            themeWarningLog() << "Couldn't load built-in icon theme" << id;
            continue;
        }

        addIconTheme(std::move(theme));
        themeDebugLog() << "Loaded Built-In Icon Theme" << id;
    }

    if (!m_iconThemeFolder.mkpath("."))
        themeWarningLog() << "Couldn't create icon theme folder";
    themeDebugLog() << "Icon Theme Folder Path:" << m_iconThemeFolder.absolutePath();

    QDirIterator directoryIterator(m_iconThemeFolder.path(), QDir::Dirs | QDir::NoDotAndDotDot);
    while (directoryIterator.hasNext()) {
        QDir dir(directoryIterator.next());
        IconTheme theme(dir.dirName(), dir.path());
        if (!theme.load())
            continue;

        addIconTheme(std::move(theme));
        themeDebugLog() << "Loaded Custom Icon Theme from" << dir.path();
    }

    themeDebugLog() << "<> Icon themes initialized.";
}

void ThemeManager::installBundledThemes()
{
    const QDir bundled(QDir(QCoreApplication::applicationDirPath()).filePath("themes"));
    if (!bundled.exists()) {
        themeDebugLog() << "No bundled themes folder next to the executable.";
        return;
    }

    if (!m_applicationThemeFolder.mkpath(".")) {
        themeWarningLog() << "Couldn't create theme folder for bundled install";
        return;
    }

    QDirIterator directoryIterator(bundled.path(), QDir::Dirs | QDir::NoDotAndDotDot);
    while (directoryIterator.hasNext()) {
        const QDir srcDir(directoryIterator.next());
        const QString id = srcDir.dirName();
        // Skip retired themes if somehow present next to the binary
        if (isRetiredTheme(id))
            continue;

        const QString destPath = m_applicationThemeFolder.absoluteFilePath(id);
        if (QDir(destPath).exists())
            continue;

        themeDebugLog() << "Installing bundled theme:" << id;
        try {
            FS::copy(srcDir.absolutePath(), destPath)();
        } catch (const Exception& e) {
            themeWarningLog() << "Failed to install bundled theme" << id << ":" << e.cause();
        }
    }
}

void ThemeManager::loadThemesFromFolder(const QDir& folder, const QString& darkThemeId)
{
    QDirIterator directoryIterator(folder.path(), QDir::Dirs | QDir::NoDotAndDotDot);
    while (directoryIterator.hasNext()) {
        QDir dir(directoryIterator.next());
        if (isRetiredTheme(dir.dirName()))
            continue;

        QFileInfo themeJson(dir.absoluteFilePath("theme.json"));
        if (themeJson.exists()) {
            themeDebugLog() << "Loading JSON Theme from:" << themeJson.absoluteFilePath();
            addTheme(std::make_unique<CustomTheme>(getTheme(darkThemeId), themeJson, true));
        } else {
            QDirIterator stylesheetFileIterator(dir.absoluteFilePath(""), { "*.qss", "*.css" }, QDir::Files);
            while (stylesheetFileIterator.hasNext()) {
                QFile customThemeFile(stylesheetFileIterator.next());
                QFileInfo customThemeFileInfo(customThemeFile);
                themeDebugLog() << "Loading QSS Theme from:" << customThemeFileInfo.absoluteFilePath();
                addTheme(std::make_unique<CustomTheme>(getTheme(darkThemeId), customThemeFileInfo, false));
            }
        }
    }
}

void ThemeManager::initializeWidgets()
{
    themeDebugLog() << "<> Initializing Widget Themes";
    themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::make_unique<SystemTheme>(m_defaultStyle, m_defaultPalette, true));
    auto darkThemeId = addTheme(std::make_unique<DarkTheme>());
    themeDebugLog() << "Loading Built-in Theme:" << darkThemeId;
    themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::make_unique<BrightTheme>());
    themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::make_unique<FusionTheme>());
    themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::make_unique<Win7AeroTheme>());
    themeDebugLog() << "Loading Built-in Theme:" << addTheme(std::make_unique<XylarDarkTheme>());

    themeDebugLog() << "<> Initializing System Widget Themes";
    QStringList styles = QStyleFactory::keys();
    for (auto& st : styles) {
#ifdef Q_OS_WINDOWS
        if (QSysInfo::productVersion() != "11" && st == "windows11") {
            continue;
        }
#endif
        // "Fusion" is already provided as a first-class built-in theme above.
        if (st.compare("fusion", Qt::CaseInsensitive) == 0)
            continue;
        themeDebugLog() << "Loading System Theme:" << addTheme(std::make_unique<SystemTheme>(st, m_defaultPalette, false));
    }

    if (!m_applicationThemeFolder.mkpath("."))
        themeWarningLog() << "Couldn't create theme folder";
    themeDebugLog() << "Theme Folder Path:" << m_applicationThemeFolder.absolutePath();

    installBundledThemes();
    loadThemesFromFolder(m_applicationThemeFolder, darkThemeId);

    themeDebugLog() << "<> Widget themes initialized.";
}

#ifndef Q_OS_MACOS
void ThemeManager::setTitlebarColorOnMac(WId windowId, QColor color) {}
void ThemeManager::setTitlebarColorOfAllWindowsOnMac(QColor color) {}
void ThemeManager::stopSettingNewWindowColorsOnMac() {}
#endif

QList<IconTheme*> ThemeManager::getValidIconThemes()
{
    QList<IconTheme*> ret;
    ret.reserve(m_icons.size());
    for (auto&& [id, theme] : m_icons) {
        ret.append(&theme);
    }
    return ret;
}

QList<ITheme*> ThemeManager::getValidApplicationThemes()
{
    QList<ITheme*> ret;
    ret.reserve(m_themes.size());
    for (auto&& [id, theme] : m_themes) {
        ret.append(theme.get());
    }
    return ret;
}

bool ThemeManager::isValidIconTheme(const QString& id)
{
    return !id.isEmpty() && m_icons.find(id) != m_icons.end();
}

bool ThemeManager::isValidApplicationTheme(const QString& id)
{
    if (id.isEmpty())
        return false;
    if (m_themes.find(id) != m_themes.end())
        return true;
    for (const auto& [tid, theme] : m_themes) {
        if (theme->name().compare(id, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

QDir ThemeManager::getIconThemesFolder()
{
    return m_iconThemeFolder;
}

QDir ThemeManager::getApplicationThemesFolder()
{
    return m_applicationThemeFolder;
}

void ThemeManager::setIconTheme(const QString& name)
{
    QString themeName = name;
    if (themeName.isEmpty() || m_icons.find(themeName) == m_icons.end()) {
        if (m_icons.find("pe_light") != m_icons.end()) {
            themeName = "pe_light";
        }
    }
    if (m_icons.find(themeName) == m_icons.end()) {
        themeWarningLog() << "Tried to set invalid icon theme:" << themeName;
        return;
    }

    QIcon::setThemeName(themeName);
}

void ThemeManager::setApplicationTheme(const QString& name, bool initial)
{
    auto themeIter = m_themes.find(name);
    if (themeIter == m_themes.end()) {
        for (auto it = m_themes.begin(); it != m_themes.end(); ++it) {
            if (it->second->name().compare(name, Qt::CaseInsensitive) == 0) {
                themeIter = it;
                break;
            }
        }
    }
    if (themeIter != m_themes.end()) {
        auto& theme = themeIter->second;
        themeDebugLog() << "applying theme" << theme->name();
        theme->apply(initial);
        setTitlebarColorOfAllWindowsOnMac(qApp->palette().window().color());

        m_logColors = theme->logColorScheme();
    } else {
        themeWarningLog() << "Tried to set invalid theme:" << name;
    }
}

void ThemeManager::applyCurrentlySelectedTheme(bool initial)
{
    auto settings = APPLICATION->settings();
    auto iconTheme = settings->get("IconTheme").toString();
    if (iconTheme.isEmpty() || !isValidIconTheme(iconTheme)) {
        iconTheme = "pe_light";
    }
    setIconTheme(iconTheme);
    themeDebugLog() << "<> Icon theme set.";
    auto applicationTheme = settings->get("ApplicationTheme").toString();
    if (applicationTheme.isEmpty() || !isValidApplicationTheme(applicationTheme)) {
        if (isValidApplicationTheme("xylar_dark")) {
            applicationTheme = "xylar_dark";
        } else if (isValidApplicationTheme("Breeze-Dark")) {
            applicationTheme = "Breeze-Dark";
        } else if (isValidApplicationTheme("Breeze Dark")) {
            applicationTheme = "Breeze Dark";
        } else {
            applicationTheme = m_defaultStyle;
        }
    }
    setApplicationTheme(applicationTheme, initial);
    themeDebugLog() << "<> Application theme set.";
}

QString ThemeManager::getThemeSideImagePath() const
{
    const QStringList searchPaths = QDir::searchPaths("theme");
    for (const QString& path : searchPaths) {
        const QString side = QDir(path).filePath("side.png");
        if (QFileInfo::exists(side))
            return side;
        const QString sideJpg = QDir(path).filePath("side.jpg");
        if (QFileInfo::exists(sideJpg))
            return sideJpg;
    }
    return {};
}

void ThemeManager::refresh()
{
    m_themes.clear();
    m_icons.clear();

    initializeThemes();
}
