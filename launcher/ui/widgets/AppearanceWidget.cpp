#include "AppearanceWidget.h"
#include "ui_AppearanceWidget.h"

#include <DesktopServices.h>
#include <QIcon>
#include <QImage>
#include <QPushButton>
#include "BuildConfig.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"

#include <Application.h>
#include "settings/SettingsObject.h"

AppearanceWidget::AppearanceWidget(bool themesOnly, QWidget* parent)
    : QWidget(parent), m_ui(new Ui::AppearanceWidget), m_themesOnly(themesOnly)
{
    m_ui->setupUi(this);

    m_defaultFormat = QTextCharFormat(m_ui->consolePreview->currentCharFormat());

    if (themesOnly) {
        m_ui->settingsBox->hide();
        m_ui->consolePreview->hide();
        loadThemeSettings();
    } else {
        loadSettings();
        loadThemeSettings();
        updateConsolePreview();
    }

    connect(m_ui->fontSizeBox, &QSpinBox::valueChanged, this, &AppearanceWidget::updateConsolePreview);
    connect(m_ui->consoleFont, &QFontComboBox::currentFontChanged, this, &AppearanceWidget::updateConsolePreview);

    connect(m_ui->iconsComboBox, &QComboBox::currentIndexChanged, this, &AppearanceWidget::applyIconTheme);
    connect(m_ui->widgetStyleComboBox, &QComboBox::currentIndexChanged, this, &AppearanceWidget::applyWidgetTheme);

    connect(m_ui->iconsFolder, &QPushButton::clicked, this,
            [] { DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path()); });
    connect(m_ui->widgetStyleFolder, &QPushButton::clicked, this,
            [] { DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path()); });
    connect(m_ui->reloadThemesButton, &QPushButton::pressed, this, &AppearanceWidget::loadThemeSettings);

    updateIconPreview();
}

void AppearanceWidget::updateIconPreview()
{
    // Only show icons the selected icon theme actually has: no empty buttons in the preview.
    const QList<QPushButton*> buttons = { m_ui->icon1, m_ui->icon2, m_ui->icon3, m_ui->icon4, m_ui->icon5,
                                          m_ui->icon6, m_ui->icon7, m_ui->icon8, m_ui->icon9 };
    for (QPushButton* button : buttons) {
        QString name = button->property("themeIconName").toString();
        if (name.isEmpty()) {
            name = button->icon().name();
            button->setProperty("themeIconName", name);
        }
        bool available = false;
        if (!name.isEmpty() && QIcon::hasThemeIcon(name)) {
            const QIcon icon = QIcon::fromTheme(name);
            // A theme can list an icon it can't draw; only keep buttons that show something.
            const QImage image = icon.pixmap(24, 24).toImage().convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < image.height() && !available; y++) {
                for (int x = 0; x < image.width(); x++) {
                    if (qAlpha(image.pixel(x, y)) > 0) {
                        available = true;
                        break;
                    }
                }
            }
            if (available)
                button->setIcon(icon);
        }
        button->setVisible(available);
    }
    // The last one repeated "settings".
    m_ui->icon10->hide();
}

AppearanceWidget::~AppearanceWidget()
{
    delete m_ui;
}

void AppearanceWidget::applySettings()
{
    SettingsObject* settings = APPLICATION->settings();
    QString consoleFontFamily = m_ui->consoleFont->currentFont().family();
    settings->set("ConsoleFont", consoleFontFamily);
    settings->set("ConsoleFontSize", m_ui->fontSizeBox->value());
}

void AppearanceWidget::loadSettings()
{
    SettingsObject* settings = APPLICATION->settings();
    QString fontFamily = settings->get("ConsoleFont").toString();
    QFont consoleFont(fontFamily);
    m_ui->consoleFont->setCurrentFont(consoleFont);

    bool conversionOk = true;
    int fontSize = settings->get("ConsoleFontSize").toInt(&conversionOk);
    if (!conversionOk) {
        fontSize = 11;
    }
    m_ui->fontSizeBox->setValue(fontSize);
}

void AppearanceWidget::retranslateUi()
{
    m_ui->retranslateUi(this);
}

void AppearanceWidget::applyIconTheme(int index)
{
    auto settings = APPLICATION->settings();
    auto originalIconTheme = settings->get("IconTheme").toString();
    auto newIconTheme = m_ui->iconsComboBox->itemData(index).toString();
    if (originalIconTheme != newIconTheme) {
        settings->set("IconTheme", newIconTheme);
        APPLICATION->themeManager()->applyCurrentlySelectedTheme();
    }
    updateIconPreview();
}

void AppearanceWidget::applyWidgetTheme(int index)
{
    auto settings = APPLICATION->settings();
    auto originalAppTheme = settings->get("ApplicationTheme").toString();
    auto newAppTheme = m_ui->widgetStyleComboBox->itemData(index).toString();
    if (originalAppTheme != newAppTheme) {
        settings->set("ApplicationTheme", newAppTheme);
        APPLICATION->themeManager()->applyCurrentlySelectedTheme();
    }

    updateConsolePreview();
}

void AppearanceWidget::loadThemeSettings()
{
    APPLICATION->themeManager()->refresh();

    m_ui->iconsComboBox->blockSignals(true);
    m_ui->widgetStyleComboBox->blockSignals(true);

    m_ui->iconsComboBox->clear();
    m_ui->widgetStyleComboBox->clear();

    SettingsObject* settings = APPLICATION->settings();

    const QString currentIconTheme = settings->get("IconTheme").toString();
    const auto iconThemes = APPLICATION->themeManager()->getValidIconThemes();

    for (int i = 0; i < iconThemes.count(); ++i) {
        const IconTheme* theme = iconThemes[i];

        QIcon iconForComboBox = QIcon(theme->path() + "/scalable/settings");
        m_ui->iconsComboBox->addItem(iconForComboBox, theme->name(), theme->id());

        if (currentIconTheme == theme->id())
            m_ui->iconsComboBox->setCurrentIndex(i);
    }

    const QString currentTheme = settings->get("ApplicationTheme").toString();
    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();
    for (int i = 0; i < themes.count(); ++i) {
        ITheme* theme = themes[i];

        m_ui->widgetStyleComboBox->addItem(theme->name(), theme->id());

        if (!theme->tooltip().isEmpty())
            m_ui->widgetStyleComboBox->setItemData(i, theme->tooltip(), Qt::ToolTipRole);

        if (currentTheme == theme->id() || currentTheme == theme->name())
            m_ui->widgetStyleComboBox->setCurrentIndex(i);
    }

    m_ui->iconsComboBox->blockSignals(false);
    m_ui->widgetStyleComboBox->blockSignals(false);
}

void AppearanceWidget::updateConsolePreview()
{
    const LogColors& colors = APPLICATION->themeManager()->getLogColors();

    int fontSize = m_ui->fontSizeBox->value();
    QString fontFamily = m_ui->consoleFont->currentFont().family();
    m_ui->consolePreview->clear();
    m_defaultFormat.setFont(QFont(fontFamily, fontSize));

    auto print = [this, colors](const QString& message, MessageLevel level) {
        QTextCharFormat format(m_defaultFormat);

        QColor bg = colors.background.value(level);
        QColor fg = colors.foreground.value(level);

        if (bg.isValid())
            format.setBackground(bg);

        if (fg.isValid())
            format.setForeground(fg);

        auto workCursor = m_ui->consolePreview->textCursor();
        workCursor.movePosition(QTextCursor::End);
        workCursor.insertText(message, format);
        workCursor.insertBlock();
    };

    print(QString("%1 version: %2\n").arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.printableVersionString()), MessageLevel::Launcher);
    print(tr("[ERROR] XylarJava detected an error!! oops!"), MessageLevel::Error);
    print(tr("[INFO] A harmless message..."), MessageLevel::Info);
    print(tr("[WARN] stay out of the way.. this is a warning"), MessageLevel::Warning);
    print(tr("[DEBUG] A secret debugging message..."), MessageLevel::Debug);
    print(tr("[FATAL] A terrifying fatal error!"), MessageLevel::Fatal);
}
