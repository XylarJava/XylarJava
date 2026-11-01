#pragma once

#include <QDialog>
#include <memory>

#include <translations/TranslationsModel.h>
#include "java/JavaChecker.h"
#include "ui/pages/BasePage.h"

class QTextCharFormat;
class SettingsObject;

namespace Ui {
class LauncherPage;
}

class LauncherPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit LauncherPage(QWidget* parent = 0);
    ~LauncherPage();

    QString displayName() const override { return tr("General"); }
    QIcon icon() const override { return QIcon::fromTheme("settings"); }
    QString id() const override { return "launcher-settings"; }
    QString helpPage() const override { return "Launcher-settings"; }
    bool apply() override;
    void retranslate() override;

   private:
    void applySettings();
    void loadSettings();

   private slots:
    void on_instDirBrowseBtn_clicked();
    void on_modsDirBrowseBtn_clicked();
    void on_iconsDirBrowseBtn_clicked();
    void on_downloadsDirBrowseBtn_clicked();
    void on_javaDirBrowseBtn_clicked();
    void on_skinsDirBrowseBtn_clicked();
    void on_metadataEnableBtn_clicked();

   private:
    Ui::LauncherPage* ui;
    class QCheckBox* m_discordEnabled = nullptr;
    class QLineEdit* m_discordClientId = nullptr;
    class QCheckBox* m_autoUpdate = nullptr;
};
