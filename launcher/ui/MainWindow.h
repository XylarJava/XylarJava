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
class XylarSkinClient;
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

    /** Takes the old toolbars and status bar out of the window. */
    void hideLegacyBars();

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
    /// A hosted server started, stopped or got its address.
    void onLocalServerChanged();

   private:
    void retranslateUi();

    /// Apply XylarJava's accent styling to the toolbars and instance view.
    void applyXylarBranding();

    enum class Page { Home = 0, Instances = 1, Servers = 2, Settings = 3 };
    /// Show the global settings inside the window, on the given settings page.
    void openSettings(const QString& pageId);
    /// Save and close the embedded settings. Returns false if a page refused to close.
    bool closeSettings();
    /// Switch the central area between the Home screen and the instance list.
    void switchToPage(Page page);
    /// Show/hide page-specific chrome (the instance toolbar only belongs to the instance list).
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
    /// Tells offline servers the players' names as they're really spelled.
    void updateKnownPlayers();
    /// The Social button and panel only show on the Home screen.
    void updateSocialVisibility();
    /// Tell XylarJava Social what the player is doing (launcher, playing, away).
    void updateSocialActivity();
    /// Who Social is online as: the player in the running game, else the launcher's account.
    void updateSocialPlayer();
    /// Watch every hosted server so Social and Discord know when one runs.
    void watchLocalServers();
    /// The first server this computer hosts right now, or nullptr.
    class LocalServer* hostedServer() const;
    /// The address friends join @p server with (its playit.gg address, else the public one); empty when there's none yet.
    QString friendAddress(class LocalServer* server) const;
    /// The server the game is on, when friends may see it (hostnames only; our own server by its playit address).
    QString sharedGameServer() const;
    /// The server the game is on, as Social and Discord show it ("My server", "play.example.com", "a server").
    QString gameServerLabel() const;
    /// Whitelists the friend on the hosted server and sends them an invite.
    void inviteFriend(const QString& name);
    /// Starts an instance of @p version (made when there's none) straight onto @p server.
    void joinServer(const QString& server, const QString& version, const QString& friendName);
    /// Shows a desktop notification for something that happened in Social.
    void showSocialNotice(int kind, const QString& name, const QString& detail);
    /// Brings the window up with the Social panel open.
    void showSocialPanel();
    /// Opens @p name's XylarJava profile (yours or a friend's).
    void showPlayerProfile(const QString& name);
    /// Window state with the instance toolbar's user-chosen visibility, regardless of the current page.

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
    class SocialClient* m_social = nullptr;
    class SocialPanel* m_socialPanel = nullptr;
    class SocialButton* m_socialButton = nullptr;
    class ProfileClient* m_profiles = nullptr;
    QTimer* m_awayTimer = nullptr;
    bool m_away = false;
    bool m_defaultSkinDownloadTried = false;  // fetched the default skins once this session
    QString m_homeSkinKey;  // Mojang lookup the Home screen is waiting for
    QString m_homeXylarSkinUuid;
    QString m_homeXylarSkinRequest;
    XylarSkinClient* m_homeSkinClient = nullptr;
    QWidget* m_settingsPage = nullptr;
    class LocalServerList* m_localServers = nullptr;
    class LocalServersPage* m_serversPage = nullptr;
    PageContainer* m_settingsContainer = nullptr;
    std::unique_ptr<SettingsObjectLockHolder> m_settingsLock;
    BaseInstance* m_playingInstance = nullptr;
    class GameActivity* m_game = nullptr;
    class DesktopNotifier* m_notifier = nullptr;
    qint64 m_launcherStartedAt = 0;  ///< seconds since the epoch, for Discord's "elapsed"
    Page m_currentPage = Page::Home;
    QString m_appliedChromeStyle;

    BaseInstance* m_selectedInstance = nullptr;
    QString m_currentInstIcon;

    // managed by the application object
    Task* m_versionLoadTask = nullptr;
};
