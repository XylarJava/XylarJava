#pragma once

#include <memory>

#include <QMainWindow>
#include <QProcess>
#include <QTimer>

#include "BaseInstance.h"
#include "minecraft/auth/MinecraftAccount.h"

class LaunchController;
class QToolButton;
class InstanceProxyModel;
class LabeledToolButton;
class QLabel;
class MinecraftLauncher;
class BaseProfilerFactory;
class InstanceView;
class InstanceTask;
class LabeledToolButton;
class HomePage;
class SideBar;
class FadingStackedWidget;
class DiscordPresence;
class XylarUpdater;
class MojangSkinFetcher;
class PageContainer;
class SettingsObjectLockHolder;
class QAction;
class QMenu;

namespace Ui {
class MainWindow;
}
class MainWindow : public QMainWindow {
    Q_OBJECT

   public:
    explicit MainWindow(QWidget* parent = 0);
    ~MainWindow();

    bool eventFilter(QObject* obj, QEvent* ev) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

    void checkInstancePathForProblems();

    void updatesAllowedChanged(bool allowed);

    void processURLs(QList<QUrl> urls);
   signals:
    void isClosing();

   protected:
    QMenu* createPopupMenu() override;
    void showEvent(QShowEvent* event) override;

   private slots:
    void on_actionAbout_triggered();

    void on_actionAddInstance_triggered();

    void on_actionREDDIT_triggered();

    void on_actionMATRIX_triggered();

    void on_actionDISCORD_triggered();

    void on_actionCopyInstance_triggered();

    void on_actionChangeInstGroup_triggered();

    void on_actionChangeInstIcon_triggered();

    void on_actionViewLauncherRootFolder_triggered();

    void on_actionViewInstanceFolder_triggered();
    void on_actionViewCentralModsFolder_triggered();

    void on_actionViewIconThemeFolder_triggered();
    void on_actionViewWidgetThemeFolder_triggered();
    void on_actionViewIconsFolder_triggered();
    void on_actionViewLogsFolder_triggered();
    void on_actionViewJavaFolder_triggered();

    void on_actionViewSkinsFolder_triggered();

    void on_actionViewSelectedInstFolder_triggered();

    void refreshInstances();

    void checkForUpdates();

    void on_actionSettings_triggered();

    void on_actionManageAccounts_triggered();

    void on_actionReportBug_triggered();

    void on_actionClearMetadata_triggered();

#ifdef Q_OS_MAC
    void on_actionAddToPATH_triggered();
#endif

    void on_actionOpenWiki_triggered();

    void on_actionMoreNews_triggered();

    void newsButtonClicked();

    void on_actionLaunchInstance_triggered();

    void on_actionKillInstance_triggered();

    void on_actionDeleteInstance_triggered();

    void deleteGroup(QString group);
    void renameGroup(QString group);
    void undoTrashInstance();

    inline void on_actionExportInstance_triggered() { on_actionExportInstanceZip_triggered(); }
    void on_actionExportInstanceZip_triggered();
    void on_actionExportInstanceMrPack_triggered();
    void on_actionExportInstanceFlamePack_triggered();

    void on_actionRenameInstance_triggered();

    void on_actionEditInstance_triggered();

    void on_actionCreateInstanceShortcut_triggered();

    void taskEnd();

    /**
     * called when an icon is changed in the icon model.
     */
    void iconUpdated(QString);

    void showInstanceContextMenu(const QPoint&);

    void updateMainToolBar();

    void updateLaunchButton();

    void updateThemeMenu();

    void instanceActivated(QModelIndex);

    void instanceChanged(const QModelIndex& current, const QModelIndex& previous);

    void instanceSelectRequest(QString id);

    void instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

    void selectionBad();

    void startTask(Task* task);

    void defaultAccountChanged();

    void changeActiveAccount();

    void repopulateAccountsMenu();

    void updateNewsLabel();

    void globalSettingsClosed();

    void setStatusBarVisibility(bool);

    void lockToolbars(bool);

#ifndef Q_OS_MAC
    void keyReleaseEvent(QKeyEvent* event) override;
#endif

    void refreshCurrentInstance();

    /// Any instance started or stopped: keeps Discord Rich Presence in sync.
    void onInstanceRunningChanged(bool running);

   private:
    void retranslateUi();

    /// Apply XylarJava's accent styling to the toolbars and instance view.
    void applyXylarBranding();

    enum class Page { Home = 0, Instances = 1, Settings = 2 };
    /// Show the global settings inside the window, on the given settings page.
    void openSettings(const QString& pageId);
    /// Save and close the embedded settings. Returns false if a page refused to close.
    bool closeSettings();
    /// Switch the central area between the Home screen and the instance list.
    void switchToPage(Page page);
    /// Show/hide page-specific chrome (the instance toolbar only belongs to the instance list).
    void applyPageChrome();
    /// Refresh the launch card on the Home screen from the selected instance.
    void updateHomePage();
    /// Fill the Home screen's instance picker menu.
    void populateHomeInstanceMenu();
    void updateHomeRecent();
    void setUpXylarClient();
    BaseInstance* instanceById(const QString& id) const;
    /// Show the active account and the skin it will have in game on the Home screen.
    void updateHomePlayer();
    /// Apply the Discord settings and show the right status.
    void updateDiscordPresence();
    /// Watch every instance so Discord knows when one is running.
    void watchInstancesForPresence();
    /// Window state with the instance toolbar's user-chosen visibility, regardless of the current page.
    QString serializedWindowState();

    void addInstance(const QString& url = QString(), const QMap<QString, QString>& extra_info = {});
    void activateInstance(BaseInstance* instance);
    void updateInstanceToolIcon(QString new_icon);
    void setSelectedInstanceById(const QString& id);
    void updateStatusCenter();
    void setInstanceActionsEnabled(bool enabled);

    void runModalTask(Task* task);
    void instanceFromInstanceTask(InstanceTask* task);

   private:
    Ui::MainWindow* ui;
    // these are managed by Qt's memory management model!
    InstanceView* view = nullptr;
    InstanceProxyModel* proxymodel = nullptr;
    QToolButton* newsLabel = nullptr;
    QLabel* m_statusLeft = nullptr;
    QLabel* m_statusCenter = nullptr;
    LabeledToolButton* changeIconButton = nullptr;
    LabeledToolButton* renameButton = nullptr;
    std::shared_ptr<Setting> instanceToolbarSetting = nullptr;

    SideBar* m_sideBar = nullptr;
    FadingStackedWidget* m_pages = nullptr;
    HomePage* m_homePage = nullptr;
    QMenu* m_homeInstanceMenu = nullptr;
    QAction* m_addAccountAction = nullptr;
    DiscordPresence* m_discord = nullptr;
    XylarUpdater* m_updater = nullptr;
    MojangSkinFetcher* m_skinFetcher = nullptr;
    QString m_homeSkinKey;  // Mojang lookup the Home screen is waiting for
    QWidget* m_settingsPage = nullptr;
    PageContainer* m_settingsContainer = nullptr;
    std::unique_ptr<SettingsObjectLockHolder> m_settingsLock;
    BaseInstance* m_playingInstance = nullptr;
    Page m_currentPage = Page::Home;
    bool m_instanceToolBarWanted = true;
    bool m_pageChromeReady = false;
    QString m_appliedChromeStyle;

    BaseInstance* m_selectedInstance = nullptr;
    QString m_currentInstIcon;

    // managed by the application object
    Task* m_versionLoadTask = nullptr;
};
