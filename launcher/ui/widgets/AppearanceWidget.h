#pragma once

#include <QDialog>

#include <translations/TranslationsModel.h>
#include <QTextCursor>

class QTextCharFormat;
class SettingsObject;

namespace Ui {
class AppearanceWidget;
}

class AppearanceWidget : public QWidget {
    Q_OBJECT

   public:
    explicit AppearanceWidget(bool simple, QWidget* parent = 0);
    virtual ~AppearanceWidget();

   public:
    void applySettings();
    void loadSettings();
    void retranslateUi();

   private:
    void applyIconTheme(int index);
    void applyWidgetTheme(int index);
    void loadThemeSettings();

    void updateConsolePreview();

    Ui::AppearanceWidget* m_ui;
    QTextCharFormat m_defaultFormat;
    bool m_themesOnly;
};
