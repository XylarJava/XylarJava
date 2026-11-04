#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QVariant>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressDialog>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
#include <QDateTime>
#include <QMessageBox>
#include <QPointer>
#include <QSet>
#include <algorithm>
#include <memory>

#include <BaseInstance.h>
#include <BuildConfig.h>
#include <DesktopServices.h>
#include <InstanceList.h>
#include <MMCZip.h>
#include <icons/IconList.h>
#include <java/JavaInstallList.h>
#include <java/JavaUtils.h>
#include <launch/LaunchTask.h>
#include <minecraft/MinecraftInstance.h>
#include <minecraft/auth/AccountList.h>
#include <minecraft/skins/XylarSkinClient.h>
#include <net/ApiDownload.h>
#include <net/NetJob.h>
#include <tools/BaseProfiler.h>
#include <updater/ExternalUpdater.h>
#include <updater/XylarUpdater.h>
#include "InstanceWindow.h"

#include "ui/GuiUtil.h"
#include "ui/ViewLogWindow.h"
#include "ui/dialogs/AboutDialog.h"
#include "ui/dialogs/skins/XylarSkinsPage.h"
#include "ui/dialogs/CopyInstanceDialog.h"
#include "ui/dialogs/CreateShortcutDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/ExportInstanceDialog.h"
#include "ui/dialogs/ExportPackDialog.h"
#include "ui/dialogs/IconPickerDialog.h"
#include "ui/dialogs/ImportResourceDialog.h"
#include "ui/dialogs/NewInstanceDialog.h"
#include "ui/dialogs/NewsDialog.h"
#include "ui/dialogs/ProgressDialog.h"
#include "ui/instanceview/InstanceDelegate.h"
#include "ui/instanceview/InstanceProxyModel.h"
#include "ui/instanceview/InstanceView.h"
#include "ui/themes/ITheme.h"
#include "ui/themes/ThemeManager.h"
#include "discord/DiscordPresence.h"
#include "minecraft/skins/DefaultSkin.h"
#include "minecraft/skins/MojangSkinFetcher.h"
#include "ui/widgets/FadingStackedWidget.h"
#include "ui/widgets/PageContainer.h"
#include "settings/SettingsObject.h"
#include "ui/widgets/HomePage.h"
#include "ui/widgets/SideBar.h"
#include "ui/widgets/SocialPanel.h"
#include "social/SocialClient.h"
#include "social/ProfileClient.h"
#include "ui/dialogs/PlayerProfileDialog.h"
#include "social/DesktopNotifier.h"
#include "social/GameActivity.h"
#include "minecraft/VanillaInstanceCreationTask.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "servers/LocalServer.h"
#include "MemoryTrim.h"
#include "ui/widgets/SkinPreview.h"
#include "ui/widgets/LabeledToolButton.h"

#include "minecraft/PackProfile.h"
#include "minecraft/XylarClient.h"
#include "servers/LocalServerList.h"
#include "ui/pages/servers/LocalServersPage.h"
#include "minecraft/VersionFile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ResourcePackFolderModel.h"
#include "minecraft/mod/ShaderPackFolderModel.h"
#include "minecraft/mod/TexturePackFolderModel.h"
#include "minecraft/mod/tasks/LocalResourceParse.h"

#include "modplatform/ModIndex.h"
#include "modplatform/flame/FlameAPI.h"
#include "modplatform/flame/FlameModIndex.h"


#include "InstanceCopyTask.h"
#include "InstanceDirUpdate.h"

#include "Json.h"

#include "MMCTime.h"

// Keeps settings from being written to disk piece by piece while the pages are open,
// like the settings dialog does.
class SettingsObjectLockHolder {
   public:
    explicit SettingsObjectLockHolder(SettingsObject* settings) : m_lock(settings) {}

   private:
    SettingsObject::Lock m_lock;
};


namespace {
QString profileInUseFilter(const QString& profile, bool used)
{
    if (used) {
        return QObject::tr("%1 (in use)").arg(profile);
    } else {
        return profile;
    }
}
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowIcon(APPLICATION->logo());
    setWindowTitle(APPLICATION->applicationDisplayName());
#ifndef QT_NO_ACCESSIBILITY
    setAccessibleName(BuildConfig.LAUNCHER_DISPLAYNAME);
#endif

    // instance toolbar stuff
    {
        // Qt doesn't like vertical moving toolbars, so we have to force them...
        // See https://github.com/PolyMC/PolyMC/issues/493
        connect(ui->instanceToolBar, &QToolBar::orientationChanged,
                [this](Qt::Orientation) { ui->instanceToolBar->setOrientation(Qt::Vertical); });

        // if you try to add a widget to a toolbar in a .ui file
        // qt designer will delete it when you save the file >:(
        // Instance icon editing is intentionally disabled in XylarJava: the
        // button is kept as a valid (hidden) object so icon-update callbacks stay
        // safe, but it is never shown in the toolbar.
        changeIconButton = new LabeledToolButton(this);
        changeIconButton->setObjectName(QStringLiteral("changeIconButton"));
        changeIconButton->setIcon(QIcon::fromTheme("news"));
        changeIconButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        changeIconButton->setVisible(false);

        renameButton = new LabeledToolButton(this);
        renameButton->setObjectName(QStringLiteral("renameButton"));
        renameButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        connect(renameButton, &QToolButton::clicked, this, &MainWindow::on_actionRenameInstance_triggered);
        ui->instanceToolBar->insertWidgetBefore(ui->actionLaunchInstance, renameButton);

        ui->instanceToolBar->insertSeparator(ui->actionLaunchInstance);

        // restore the instance toolbar settings
        auto const setting_name = QString("WideBarVisibility_%1").arg(ui->instanceToolBar->objectName());
        instanceToolbarSetting = APPLICATION->settings()->getOrRegisterSetting(setting_name);

        ui->instanceToolBar->setVisibilityState(QByteArray::fromBase64(instanceToolbarSetting->get().toString().toUtf8()));

        ui->instanceToolBar->addContextMenuAction(ui->newsToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->instanceToolBar->toggleViewAction());
        ui->instanceToolBar->addContextMenuAction(ui->actionToggleStatusBar);
        ui->instanceToolBar->addContextMenuAction(ui->actionLockToolbars);
    }

    // set the menu for the folders, accounts, and export tool buttons
    {
        auto foldersMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionFoldersButton));
        ui->actionFoldersButton->setMenu(ui->foldersMenu);
        foldersMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto accountMenuButton = dynamic_cast<QToolButton*>(ui->mainToolBar->widgetForAction(ui->actionAccountsButton));
        accountMenuButton->setPopupMode(QToolButton::InstantPopup);

        auto exportInstanceMenu = new QMenu(this);
        exportInstanceMenu->addAction(ui->actionExportInstanceZip);
        exportInstanceMenu->addAction(ui->actionExportInstanceMrPack);
        exportInstanceMenu->addAction(ui->actionExportInstanceFlamePack);
        ui->actionExportInstance->setMenu(exportInstanceMenu);
    }

    // hide, disable and show stuff
    {
        ui->actionReportBug->setVisible(!BuildConfig.BUG_TRACKER_URL.isEmpty());
        ui->actionMATRIX->setVisible(!BuildConfig.MATRIX_URL.isEmpty());
        ui->actionDISCORD->setVisible(!BuildConfig.DISCORD_URL.isEmpty());
        ui->actionREDDIT->setVisible(!BuildConfig.SUBREDDIT_URL.isEmpty());

        ui->actionCheckUpdate->setVisible(APPLICATION->updaterEnabled());

#ifndef Q_OS_MAC
        ui->actionAddToPATH->setVisible(false);
#endif

        // disabled until we have an instance selected
        ui->instanceToolBar->setEnabled(false);
        setInstanceActionsEnabled(false);

        // add a close button at the end of the main toolbar when running on gamescope / steam deck
        // this is only needed on gamescope because it defaults to an X11/XWayland session and
        // does not implement decorations
        if (qgetenv("XDG_CURRENT_DESKTOP") == "gamescope") {
            ui->mainToolBar->addAction(ui->actionCloseWindow);
        }

        ui->actionViewJavaFolder->setEnabled(BuildConfig.JAVA_DOWNLOADER_ENABLED);

        // Instance icon editing is disabled in XylarJava.
        ui->actionChangeInstIcon->setVisible(false);
    }

    {  // logs viewing
        connect(ui->actionViewLog, &QAction::triggered, this, [] { APPLICATION->showLogWindow(); });
    }

    // add the toolbar toggles to the view menu
    ui->viewMenu->addAction(ui->newsToolBar->toggleViewAction());

    updateThemeMenu();
    updateMainToolBar();
    // OSX magic.
    setUnifiedTitleAndToolBarOnMac(true);

    // Global shortcuts
    {
        // you can't set QKeySequence::StandardKey shortcuts in qt designer >:(
        ui->actionAddInstance->setShortcut(QKeySequence::New);
        ui->actionSettings->setShortcut(QKeySequence::Preferences);
        ui->actionUndoTrashInstance->setShortcut(QKeySequence::Undo);
        ui->actionDeleteInstance->setShortcuts({ QKeySequence(tr("Backspace")), QKeySequence::Delete });
        ui->actionCloseWindow->setShortcut(QKeySequence::Close);
        connect(ui->actionCloseWindow, &QAction::triggered, APPLICATION, &Application::closeCurrentWindow);

        // FIXME: This is kinda weird. and bad. We need some kind of managed shutdown.
        auto q = new QShortcut(QKeySequence::Quit, this);
        connect(q, &QShortcut::activated, APPLICATION, &Application::quit);
    }

    // Add the news label to the news toolbar.
    {
        newsLabel = new QToolButton();
        newsLabel->setIcon(QIcon());  // no cat/news animal icon
        newsLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        newsLabel->setToolButtonStyle(Qt::ToolButtonTextOnly);
        newsLabel->setFocusPolicy(Qt::NoFocus);
        ui->newsToolBar->addWidget(newsLabel);

        ui->actionMoreNews->setVisible(false);
        updateNewsLabel();
    }

    // Create the instance list widget
    {
        view = new InstanceView(ui->centralWidget);

        view->setSelectionMode(QAbstractItemView::SingleSelection);
        // FIXME: leaks ListViewDelegate
        auto delegate = new ListViewDelegate(this);
        view->setItemDelegate(delegate);
        view->setFrameShape(QFrame::NoFrame);
        // do not show ugly blue border on the mac
        view->setAttribute(Qt::WA_MacShowFocusRect, false);
        connect(delegate, &ListViewDelegate::textChanged, this, [this](QString before, QString after) {
            if (auto newRoot = askToUpdateInstanceDirName(m_selectedInstance, before, after, this); !newRoot.isEmpty()) {
                auto oldID = m_selectedInstance->id();
                auto newID = QFileInfo(newRoot).fileName();
                QString origGroup(APPLICATION->instances()->getInstanceGroup(oldID));
                bool syncGroup = origGroup != GroupId() && oldID != newID;
                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(oldID, GroupId());

                refreshInstances();
                setSelectedInstanceById(newID);

                if (syncGroup)
                    APPLICATION->instances()->setInstanceGroup(newID, origGroup);
            }
        });

        view->installEventFilter(this);
        view->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view, &QWidget::customContextMenuRequested, this, &MainWindow::showInstanceContextMenu);
        connect(view, &InstanceView::droppedURLs, this, &MainWindow::processURLs, Qt::QueuedConnection);

        proxymodel = new InstanceProxyModel(this);
        proxymodel->setSourceModel(APPLICATION->instances());
        proxymodel->sort(0);
        connect(proxymodel, &InstanceProxyModel::dataChanged, this, &MainWindow::instanceDataChanged);

        view->setModel(proxymodel);
        view->setSourceOfGroupCollapseStatus(
            [](const QString& groupName) -> bool { return APPLICATION->instances()->isGroupCollapsed(groupName); });
        connect(view, &InstanceView::groupStateChanged, APPLICATION->instances(), &InstanceList::on_GroupStateChanged);
    }

    // Central pages: the Home screen and the instance list
    {
        m_pages = new FadingStackedWidget(ui->centralWidget);
        m_pages->setObjectName(QStringLiteral("mainPages"));

        m_homePage = new HomePage(m_pages);
        m_homePage->setLogo(APPLICATION->logo());
        m_pages->addWidget(m_homePage);

        auto instancesPage = new QWidget(m_pages);
        instancesPage->setObjectName(QStringLiteral("instancesPage"));
        auto instancesLayout = new QVBoxLayout(instancesPage);
        instancesLayout->setContentsMargins(0, 0, 0, 0);
        instancesLayout->setSpacing(0);

        auto header = new QFrame(instancesPage);
        header->setObjectName(QStringLiteral("pageHeader"));
        auto headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(24, 18, 24, 14);
        headerLayout->setSpacing(12);
        auto title = new QLabel(tr("Your instances"), header);
        title->setObjectName(QStringLiteral("pageTitle"));
        auto addButton = new QPushButton(QIcon::fromTheme(QStringLiteral("new")), tr("Add instance"), header);
        addButton->setObjectName(QStringLiteral("addInstanceButton"));
        addButton->setCursor(Qt::PointingHandCursor);
        connect(addButton, &QPushButton::clicked, ui->actionAddInstance, &QAction::trigger);
        headerLayout->addWidget(title);
        headerLayout->addStretch(1);
        headerLayout->addWidget(addButton);

        instancesLayout->addWidget(header);
        instancesLayout->addWidget(view, 1);
        m_pages->addWidget(instancesPage);

        // Servers hosted on this computer
        m_localServers = new LocalServerList(FS::PathCombine(APPLICATION->dataRoot(), QStringLiteral("servers")), APPLICATION->network(), this);
        m_serversPage = new LocalServersPage(m_localServers, APPLICATION->network(), m_pages);
        m_serversPage->setFolderOpener([](const QString& path) { DesktopServices::openPath(path, true); });
        m_serversPage->setModSources([] {
            QList<QPair<QString, QString>> sources;
            auto instances = APPLICATION->instances();
            for (int i = 0; i < instances->count(); i++) {
                auto minecraft = dynamic_cast<MinecraftInstance*>(instances->at(i));
                if (!minecraft || !minecraft->getPackProfile())
                    continue;
                if (minecraft->getPackProfile()->getComponentVersion(QStringLiteral("net.fabricmc.fabric-loader")).isEmpty())
                    continue;
                sources << qMakePair(minecraft->name(), minecraft->modsRoot());
            }
            return sources;
        });
        m_pages->addWidget(m_serversPage);

        // Settings live inside the window too, so the sidebar never goes away.
        m_settingsPage = new QWidget(m_pages);
        m_settingsPage->setObjectName(QStringLiteral("settingsPage"));
        auto settingsLayout = new QVBoxLayout(m_settingsPage);
        settingsLayout->setContentsMargins(0, 0, 0, 0);
        settingsLayout->setSpacing(0);
        auto settingsHeader = new QFrame(m_settingsPage);
        settingsHeader->setObjectName(QStringLiteral("pageHeader"));
        auto settingsHeaderLayout = new QHBoxLayout(settingsHeader);
        settingsHeaderLayout->setContentsMargins(24, 18, 24, 14);
        auto settingsTitle = new QLabel(tr("Settings"), settingsHeader);
        settingsTitle->setObjectName(QStringLiteral("pageTitle"));
        auto settingsHint = new QLabel(tr("Changes are saved when you leave this page."), settingsHeader);
        settingsHint->setObjectName(QStringLiteral("pageHint"));
        settingsHeaderLayout->addWidget(settingsTitle);
        settingsHeaderLayout->addStretch(1);
        settingsHeaderLayout->addWidget(settingsHint);
        settingsLayout->addWidget(settingsHeader);
        m_pages->addWidget(m_settingsPage);

        // Sidebar on the left: logo, pages, quick actions and the account switcher.
        m_sideBar = new SideBar(ui->centralWidget);
        m_sideBar->setLogo(APPLICATION->logo(), BuildConfig.LAUNCHER_DISPLAYNAME);
        m_sideBar->addPage(QIcon::fromTheme(QStringLiteral("launch")), QString());
        m_sideBar->addPage(QIcon::fromTheme(QStringLiteral("minecraft")), QString());
        m_sideBar->addPage(QIcon::fromTheme(QStringLiteral("server")), QString());
        m_sideBar->addPage(QIcon::fromTheme(QStringLiteral("settings")), QString());
        // XylarJava Social: friends and who's online, in a panel that slides in on the right.
        m_social = new SocialClient(APPLICATION->network(), APPLICATION->dataRoot(), this);
        m_social->setBackendUrl(APPLICATION->settings()->get("XylarSocialUrl").toString());
        m_socialPanel = new SocialPanel(m_social, APPLICATION->network(), ui->centralWidget);
        // Its button sits in the top-right corner of the pages, out of the way of the navigation.
        m_socialButton = new SocialButton(m_pages);
        connect(m_socialButton, &QToolButton::toggled, this, [this](bool open) {
            m_socialPanel->setOpen(open);
            APPLICATION->settings()->set("SocialPanelOpen", open);
        });
        connect(m_socialPanel, &SocialPanel::closeRequested, m_socialButton, [this] { m_socialButton->setChecked(false); });
        connect(m_social, &SocialClient::changed, m_socialButton, [this] {
            m_socialButton->setBadge(m_social->incoming().size() + m_social->invites().size());
            updateKnownPlayers();
        });
        connect(m_socialPanel, &SocialPanel::inviteRequested, this, &MainWindow::inviteFriend);
        connect(m_socialPanel, &SocialPanel::joinRequested, this, &MainWindow::joinServer);
        // Profiles: time played and favourite versions, counted from the Social presence.
        m_profiles = new ProfileClient(APPLICATION->network(), m_social, this);
        m_profiles->setBackendUrl(APPLICATION->settings()->get("XylarProfilesUrl").toString());
        connect(m_socialPanel, &SocialPanel::profileRequested, this, &MainWindow::showPlayerProfile);
        // Friend requests, invites and friends coming online, also while the launcher is hidden.
        m_notifier = new DesktopNotifier(APPLICATION->logo(), FS::PathCombine(APPLICATION->dataRoot(), QStringLiteral("cache")), this);
        connect(m_notifier, &DesktopNotifier::clicked, this, &MainWindow::showSocialPanel);
        connect(m_social, &SocialClient::notice, this,
                [this](SocialClient::Notice kind, const QString& name, const QString& detail) { showSocialNotice(int(kind), name, detail); });
        m_sideBar->addActionButton(ui->actionFoldersButton);
        if (qgetenv("XDG_CURRENT_DESKTOP") == "gamescope")
            m_sideBar->addActionButton(ui->actionCloseWindow);
        m_sideBar->setAccountAction(ui->actionAccountsButton);
        connect(m_sideBar, &SideBar::pageRequested, this, [this](int index) { switchToPage(static_cast<Page>(index)); });

        ui->horizontalLayout->addWidget(m_sideBar);
        ui->horizontalLayout->addWidget(m_pages, 1);
        ui->horizontalLayout->addWidget(m_socialPanel);
        if (APPLICATION->settings()->get("SocialPanelOpen").toBool()) {
            const QSignalBlocker blocker(m_socialButton);
            m_socialButton->setChecked(true);
            m_socialPanel->setOpen(true, false);
        }

        m_homeInstanceMenu = new QMenu(this);
        m_homeInstanceMenu->setStyleSheet(QStringLiteral("QMenu { menu-scrollable: 1; }"));
        connect(m_homeInstanceMenu, &QMenu::aboutToShow, this, &MainWindow::populateHomeInstanceMenu);
        m_homePage->setInstanceMenu(m_homeInstanceMenu);

        connect(m_homePage, &HomePage::playRequested, this, &MainWindow::on_actionLaunchInstance_triggered);
        connect(m_homePage, &HomePage::stopRequested, this, &MainWindow::on_actionKillInstance_triggered);
        connect(m_homePage, &HomePage::createInstanceRequested, this, &MainWindow::on_actionAddInstance_triggered);
        connect(m_homePage, &HomePage::manageInstancesRequested, this, [this] { switchToPage(Page::Instances); });
        connect(m_homePage, &HomePage::manageAccountsRequested, this, &MainWindow::on_actionManageAccounts_triggered);
        connect(m_homePage, &HomePage::skinRequested, this, [this] {
            MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();
            if (!account) {
                on_actionManageAccounts_triggered();
                return;
            }
            XylarSkinsPage dialog(this, account);
            dialog.exec();
            updateHomePlayer();  // the skin may have changed
        });
        connect(m_homePage, &HomePage::playInstanceRequested, this, [this](const QString& id) {
            if (auto instance = instanceById(id)) {
                setSelectedInstanceById(id);
                APPLICATION->launch(instance);
            }
        });
        connect(m_homePage, &HomePage::stopInstanceRequested, this, [this](const QString& id) {
            if (auto instance = instanceById(id); instance && instance->isRunning()) {
                setSelectedInstanceById(id);
                on_actionKillInstance_triggered();
            }
        });
        connect(m_homePage, &HomePage::setupClientRequested, this, &MainWindow::setUpXylarClient);
        connect(m_homePage, &HomePage::repairInstanceRequested, this, [](const QString& id) {
            APPLICATION->repairInstance(APPLICATION->instances()->getInstanceById(id));
        });
        connect(m_homePage, &HomePage::editInstanceRequested, this, [this](const QString& id) {
            if (instanceById(id)) {
                setSelectedInstanceById(id);
                on_actionEditInstance_triggered();
            }
        });

        // Keep the Home screen in sync when instances are added, removed or reloaded.
        connect(APPLICATION->instances(), &InstanceList::rowsInserted, this, [this] { updateHomePage(); });
        connect(APPLICATION->instances(), &InstanceList::rowsRemoved, this, [this] { updateHomePage(); });
        connect(APPLICATION->instances(), &InstanceList::modelReset, this, [this] { updateHomePage(); });
    }

    // The old status bar ("No instance selected", playtime) is gone: the Home screen
    // already shows the selected instance. Its toggle goes too.
    ui->actionToggleStatusBar->setVisible(false);

    // Lock toolbars
    {
        bool toolbarsLocked = APPLICATION->settings()->get("ToolbarsLocked").toBool();
        ui->actionLockToolbars->setChecked(toolbarsLocked);
        connect(ui->actionLockToolbars, &QAction::toggled, this, &MainWindow::lockToolbars);
        lockToolbars(toolbarsLocked);
    }
    // start instance when double-clicked
    connect(view, &InstanceView::activated, this, &MainWindow::instanceActivated);

    // track the selection -- update the instance toolbar
    connect(view->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::instanceChanged);

    // track icon changes and update the toolbar!
    connect(APPLICATION->icons(), &IconList::iconUpdated, this, &MainWindow::iconUpdated);

    // model reset -> selection is invalid. All the instance pointers are wrong.
    connect(APPLICATION->instances(), &InstanceList::dataIsInvalid, this, &MainWindow::selectionBad);

    // handle newly added instances
    connect(APPLICATION->instances(), &InstanceList::instanceSelectRequest, this, &MainWindow::instanceSelectRequest);

    // When the global settings page closes, we want to know about it and update our state
    connect(APPLICATION, &Application::globalSettingsApplied, this, &MainWindow::globalSettingsClosed);

    m_statusLeft = new QLabel(tr("No instance selected"), this);
    m_statusCenter = new QLabel(tr("Total playtime: 0s"), this);
    statusBar()->addPermanentWidget(m_statusLeft, 1);
    statusBar()->addPermanentWidget(m_statusCenter, 0);

    // Add "manage accounts" button, right align
    QWidget* spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainToolBar->insertWidget(ui->actionAccountsButton, spacer);

    // The sidebar replaces the old toolbars (main, instance and news bar); keep the
    // toolbar objects (their actions live on in the sidebar and the menus) but take them out of the window.
    hideLegacyBars();
    ui->instanceToolBar->toggleViewAction()->setVisible(false);
    ui->newsToolBar->toggleViewAction()->setVisible(false);
    // With no toolbars left there's nothing to lock.
    ui->actionLockToolbars->setVisible(false);

    // No menu bar either (it used to pop up when pressing Alt). Keep the keyboard
    // shortcuts of its actions working by attaching them to the window itself.
    ui->menuBar->hide();
    for (QAction* menuAction : ui->menuBar->actions()) {
        if (QMenu* menu = menuAction->menu())
            addActions(menu->actions());
    }

    // "Add account" entry for the accounts menu
    m_addAccountAction = new QAction(this);
    m_addAccountAction->setIcon(QIcon::fromTheme(QStringLiteral("accounts")));
    connect(m_addAccountAction, &QAction::triggered, this, &MainWindow::on_actionManageAccounts_triggered);

    // Real skins from Mojang for the Home screen
    m_skinFetcher = new MojangSkinFetcher(APPLICATION->network(), this);
    connect(m_skinFetcher, &MojangSkinFetcher::finished, this, [this](const QString& key, const QImage&, bool) {
        if (key == m_homeSkinKey)
            updateHomePlayer();
    });

    // Discord Rich Presence: time in the launcher, then in game, with where the player is.
    m_launcherStartedAt = QDateTime::currentSecsSinceEpoch();
    m_discord = new DiscordPresence(this);
    m_game = new GameActivity(this);
    connect(m_game, &GameActivity::changed, this, [this] {
        updateDiscordPresence();
        updateSocialActivity();
    });
    watchLocalServers();
    if (m_localServers)
        connect(m_localServers, &LocalServerList::listChanged, this, &MainWindow::watchLocalServers);
    watchInstancesForPresence();
    connect(APPLICATION->instances(), &InstanceList::rowsInserted, this, [this] { watchInstancesForPresence(); });
    connect(APPLICATION->instances(), &InstanceList::modelReset, this, [this] { watchInstancesForPresence(); });
    updateDiscordPresence();

    // Away after ten minutes outside the launcher (unless a game is running).
    m_awayTimer = new QTimer(this);
    m_awayTimer->setSingleShot(true);
    m_awayTimer->setInterval(10 * 60 * 1000);
    connect(m_awayTimer, &QTimer::timeout, this, [this] {
        m_away = true;
        updateSocialActivity();
    });
    connect(qApp, &QGuiApplication::applicationStateChanged, this, [this](Qt::ApplicationState state) {
        if (state == Qt::ApplicationActive) {
            m_awayTimer->stop();
            if (m_away) {
                m_away = false;
                updateSocialActivity();
            }
        } else {
            if (!m_awayTimer->isActive() && !m_away)
                m_awayTimer->start();
            // The launcher went to the background: give freed memory back to the system.
            QTimer::singleShot(1500, this, [] { MemoryTrim::release(); });
        }
    });
    updateSocialActivity();

    // Startup loads and throws away a lot (instances, icons, news): trim once it has settled,
    // then now and then while the launcher stays open.
    QTimer::singleShot(8000, this, [] { MemoryTrim::release(); });
    auto trimTimer = new QTimer(this);
    connect(trimTimer, &QTimer::timeout, this, [] { MemoryTrim::release(); });
    trimTimer->start(5 * 60 * 1000);

    // GitHub release updater
    m_updater = new XylarUpdater(this, this);
    m_homePage->setVersionInfo(BuildConfig.versionString(), QString());
    connect(m_homePage, &HomePage::checkUpdatesRequested, this, [this] { m_updater->checkForUpdates(true); });
    if (!APPLICATION->updaterEnabled()) {
        ui->actionCheckUpdate->setVisible(true);
        ui->actionCheckUpdate->setEnabled(true);
        connect(ui->actionCheckUpdate, &QAction::triggered, this, [this] { m_updater->checkForUpdates(true); });
    }
    if (APPLICATION->settings()->get("XylarAutoUpdate").toBool()) {
        // Give the window a moment to settle before going online.
        QTimer::singleShot(4000, this, [this] { m_updater->checkForUpdates(false); });
    }

    // Use undocumented property... https://stackoverflow.com/questions/7121718/create-a-scrollbar-in-a-submenu-qt
    ui->accountsMenu->setStyleSheet("QMenu { menu-scrollable: 1; }");

    repopulateAccountsMenu();

    // Update the menu when the active account changes.
    // Shouldn't have to use lambdas here like this, but if I don't, the compiler throws a fit.
    // Template hell sucks...
    connect(APPLICATION->accounts(), &AccountList::defaultAccountChanged, [this] { defaultAccountChanged(); });
    connect(APPLICATION->accounts(), &AccountList::listChanged, [this] { defaultAccountChanged(); });

    // Show initial account
    defaultAccountChanged();

    // TODO: refresh accounts here?
    // auto accounts = APPLICATION->accounts();

    // News banner
    updateNewsLabel();

    if (APPLICATION->updaterEnabled()) {
        bool updatesAllowed = APPLICATION->updatesAreAllowed();
        updatesAllowedChanged(updatesAllowed);

        connect(ui->actionCheckUpdate, &QAction::triggered, this, &MainWindow::checkForUpdates);

        // set up the updater object.
        auto updater = APPLICATION->updater();

        if (updater) {
            connect(updater, &ExternalUpdater::canCheckForUpdatesChanged, this, &MainWindow::updatesAllowedChanged);
        }
    }

    connect(ui->actionUndoTrashInstance, &QAction::triggered, this, &MainWindow::undoTrashInstance);

    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());

    // removing this looks stupid
    view->setFocus();

    applyXylarBranding();

    retranslateUi();
}

void MainWindow::applyXylarBranding()
{
    // Theme-aware styling for the remaining window chrome. Colours come from the
    // active palette; the accent is picked for readability, since some themes use
    // a highlight colour close to the background.
    const QColor accent = HomePage::accentFor(palette());
    const QColor accentText = HomePage::textOn(accent);
    const QString style = QStringLiteral(
                              "QToolBar::separator { background: palette(mid); width: 1px; height: 1px; margin: 6px 8px; }"
                              "QToolBar QToolButton { border-radius: 8px; padding: 6px 10px; }"
                              "QToolBar QToolButton:hover { background-color: palette(button); }"
                              "QToolBar QToolButton:pressed { background-color: %1; color: %2; }"
                              "QToolBar#instanceToolBar { padding: 6px; spacing: 4px; border: none; border-left: 1px solid palette(mid); }"
                              "QToolBar#instanceToolBar QToolButton:checked { background-color: %1; color: %2; }"
                              "QToolBar#newsToolBar { padding: 2px 12px; border: none; }"
                              "QStatusBar QLabel { padding: 0 6px; }"
                              "#pageHeader { background: transparent; border-bottom: 1px solid palette(mid); }"
                              "#pageTitle { font-size: 22px; font-weight: 800; }"
                              "#pageHint { color: palette(placeholder-text); }"
                              "#addInstanceButton { background-color: %1; color: %2; border: none; border-radius: 10px;"
                              " padding: 8px 16px; font-weight: 700; }"
                              "#addInstanceButton:hover { background-color: %3; }")
                              .arg(accent.name(), accentText.name(), accent.lighter(112).name());

    // Only re-apply when something changed, so palette notifications can never loop.
    if (style == m_appliedChromeStyle)
        return;
    m_appliedChromeStyle = style;

    ui->instanceToolBar->setStyleSheet(style);
    ui->newsToolBar->setStyleSheet(style);
    statusBar()->setStyleSheet(style);
    if (m_pages && m_pages->count() > 1)
        m_pages->widget(1)->setStyleSheet(style);
    if (m_serversPage) {
        for (auto header : m_serversPage->findChildren<QFrame*>(QStringLiteral("pageHeader"), Qt::FindDirectChildrenOnly))
            header->setStyleSheet(style);
    }
    if (m_settingsPage) {
        // Only style the header: the settings pages keep the theme's own look.
        for (auto header : m_settingsPage->findChildren<QFrame*>(QStringLiteral("pageHeader"), Qt::FindDirectChildrenOnly))
            header->setStyleSheet(style);
    }
}

void MainWindow::switchToPage(Page page)
{
    if (page == m_currentPage && page != Page::Settings)
        return;
    if (m_currentPage == Page::Settings && page != Page::Settings && !closeSettings()) {
        // A settings page refused to close (e.g. invalid input): stay here.
        if (m_sideBar)
            m_sideBar->setCurrentPage(static_cast<int>(Page::Settings));
        return;
    }
    if (page == Page::Settings && !m_settingsContainer)
        openSettings(QStringLiteral("global-settings"));
    m_currentPage = page;
    updateSocialVisibility();
    if (m_pages)
        m_pages->setCurrentIndexAnimated(static_cast<int>(page));
    if (m_sideBar)
        m_sideBar->setCurrentPage(static_cast<int>(page));
    if (page == Page::Instances && view)
        view->setFocus();
}

void MainWindow::openSettings(const QString& pageId)
{
    if (m_settingsContainer) {
        m_settingsContainer->selectPage(pageId);
        if (m_currentPage != Page::Settings)
            switchToPage(Page::Settings);
        return;
    }
    auto provider = APPLICATION->globalSettingsProvider();
    if (!provider || !m_settingsPage)
        return;

    emit APPLICATION->globalSettingsAboutToOpen();
    m_settingsLock = std::make_unique<SettingsObjectLockHolder>(APPLICATION->settings());
    m_settingsContainer = new PageContainer(provider, pageId, m_settingsPage);
    m_settingsPage->layout()->addWidget(m_settingsContainer);

    if (m_currentPage != Page::Settings)
        switchToPage(Page::Settings);
}

bool MainWindow::closeSettings()
{
    if (!m_settingsContainer)
        return true;
    if (!m_settingsContainer->prepareToClose())
        return false;

    m_settingsPage->layout()->removeWidget(m_settingsContainer);
    m_settingsContainer->deleteLater();
    m_settingsContainer = nullptr;
    m_settingsLock.reset();
    emit APPLICATION->globalSettingsApplied();
    return true;
}

namespace {
QString describeInstance(BaseInstance* instance)
{
    if (auto minecraft = dynamic_cast<MinecraftInstance*>(instance)) {
        if (auto profile = minecraft->getPackProfile()) {
            const QString mcVersion = profile->getComponentVersion(QStringLiteral("net.minecraft"));
            if (!mcVersion.isEmpty()) {
                QStringList parts;
                parts << QObject::tr("Minecraft %1").arg(mcVersion);

                struct Loader {
                    const char* uid;
                    const char* name;
                };
                static const Loader loaders[] = {
                    { "net.fabricmc.fabric-loader", "Fabric" }, { "org.quiltmc.quilt-loader", "Quilt" },
                    { "net.neoforged", "NeoForge" },            { "net.minecraftforge", "Forge" },
                    { "com.mumfrey.liteloader", "LiteLoader" }, { "io.xylarjava.client", "XylarJava Client" },
                };
                for (const Loader& loader : loaders) {
                    const QString version = profile->getComponentVersion(QString::fromLatin1(loader.uid));
                    if (!version.isEmpty()) {
                        parts << QStringLiteral("%1 %2").arg(QString::fromLatin1(loader.name), version);
                        break;
                    }
                }
                if (parts.size() == 1)
                    parts << QObject::tr("Vanilla");

                const QString separator = QStringLiteral("  ") + QChar(0x00B7) + QStringLiteral("  ");
                return parts.join(separator);
            }
        }
    }
    return instance->getStatusbarDescription();
}
}  // namespace

void MainWindow::updateHomePage()
{
    if (!m_homePage)
        return;

    updateHomeRecent();

    if (!m_selectedInstance) {
        m_homePage->showNoInstance(APPLICATION->instances()->count() > 0);
        return;
    }

    const bool client = XylarClient::isClientInstance(m_selectedInstance);
    m_homePage->showInstance(client ? APPLICATION->logo() : APPLICATION->icons()->getIcon(m_selectedInstance->iconKey()),
                             client ? XylarClient::displayName() : m_selectedInstance->name(), describeInstance(m_selectedInstance),
                             m_selectedInstance->canLaunch(), m_selectedInstance->isRunning(), client);
}

void MainWindow::setUpXylarClient()
{
    auto task = new XylarClientCreationTask();
    task->setName(XylarClient::displayName());
    task->setIcon(QStringLiteral("default"));
    task->setGroup(APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString());
    const bool withMod = !XylarClient::findClientJar().isEmpty();
    instanceFromInstanceTask(task);

    // Select the new instance so Play on the Home screen starts it right away.
    BaseInstance* created = nullptr;
    auto instanceList = APPLICATION->instances();
    for (int i = 0; i < instanceList->count(); i++) {
        auto instance = instanceList->at(i);
        if (instance && XylarClient::isClientInstance(instance)) {
            created = instance;
            break;
        }
    }
    if (!created)
        return;  // creation failed or was cancelled; the task already told the user
    setSelectedInstanceById(created->id());
    updateHomePage();

    if (!withMod) {
        QMessageBox::information(this, XylarClient::displayName(),
                                 tr("The Minecraft 1.8.9 instance is ready.\n\n"
                                    "To add the client itself, put %1 in the \"xylarjava-client\" folder inside the launcher's data folder "
                                    "(or next to the launcher) and set it up again, or follow the steps in client/README.md.")
                                     .arg(QString::fromLatin1(XylarClient::JarFileName)));
    }
}

BaseInstance* MainWindow::instanceById(const QString& id) const
{
    return APPLICATION->instances()->getInstanceById(id);
}

namespace {
QString playedAgo(qint64 lastLaunchMs)
{
    if (lastLaunchMs <= 0)
        return QObject::tr("Never played");
    const qint64 seconds = qMax<qint64>(0, (QDateTime::currentMSecsSinceEpoch() - lastLaunchMs) / 1000);
    if (seconds < 60)
        return QObject::tr("Just now");
    if (seconds < 3600) {
        const int n = int(seconds / 60);
        return n == 1 ? QObject::tr("1 minute ago") : QObject::tr("%1 minutes ago").arg(n);
    }
    if (seconds < 86400) {
        const int n = int(seconds / 3600);
        return n == 1 ? QObject::tr("1 hour ago") : QObject::tr("%1 hours ago").arg(n);
    }
    if (seconds < 86400 * 2)
        return QObject::tr("Yesterday");
    if (seconds < 86400 * 30)
        return QObject::tr("%1 days ago").arg(int(seconds / 86400));
    return QDateTime::fromMSecsSinceEpoch(lastLaunchMs).date().toString(QStringLiteral("d MMM yyyy"));
}
}  // namespace

void MainWindow::updateHomeRecent()
{
    if (!m_homePage)
        return;

    auto instanceList = APPLICATION->instances();
    QList<BaseInstance*> sorted;
    for (int i = 0; i < instanceList->count(); i++) {
        if (auto instance = instanceList->at(i))
            sorted.append(instance);
    }
    // Running instances first, then most recently played.
    std::sort(sorted.begin(), sorted.end(), [](BaseInstance* a, BaseInstance* b) {
        if (a->isRunning() != b->isRunning())
            return a->isRunning();
        return a->lastLaunch() > b->lastLaunch();
    });

    qint64 secondsPlayed = 0;
    BaseInstance* mostPlayed = nullptr;
    for (BaseInstance* instance : sorted) {
        const qint64 played = instance->totalTimePlayed();
        secondsPlayed += played;
        if (played > 0 && (!mostPlayed || played > mostPlayed->totalTimePlayed()))
            mostPlayed = instance;
    }
    m_homePage->setStats(int(sorted.size()), secondsPlayed, mostPlayed ? mostPlayed->name() : QString());

    QList<HomePage::RecentInstance> recent;
    const QString separator = QStringLiteral("  ") + QChar(0x00B7) + QStringLiteral("  ");

    // The XylarJava Client is always pinned on top, even before it has been set up.
    BaseInstance* client = nullptr;
    for (BaseInstance* instance : sorted) {
        if (XylarClient::isClientInstance(instance) && (!client || XylarClient::hasClientComponent(instance))) {
            client = instance;
            if (XylarClient::hasClientComponent(instance))
                break;
        }
    }
    {
        HomePage::RecentInstance pinned;
        pinned.featured = true;
        pinned.icon = APPLICATION->logo();
        pinned.name = XylarClient::displayName();
        if (client) {
            pinned.id = client->id();
            pinned.running = client->isRunning();
            pinned.canLaunch = client->canLaunch();
            QString status;
            if (pinned.running)
                status = tr("Playing now");
            else if (!XylarClient::hasClientComponent(client))
                status = tr("Client mod not installed yet");
            else
                status = playedAgo(client->lastLaunch());
            pinned.details = describeInstance(client) + separator + status;
        } else {
            pinned.details = tr("Minecraft 1.8.9") + separator + tr("PvP HUD, toggle sprint, zoom") + separator + tr("Not set up yet");
        }
        recent.append(pinned);
    }

    for (BaseInstance* instance : sorted) {
        if (recent.size() >= 4)
            break;
        if (instance == client)
            continue;
        if (instance->lastLaunch() <= 0 && !instance->isRunning())
            continue;
        HomePage::RecentInstance entry;
        entry.id = instance->id();
        entry.icon = APPLICATION->icons()->getIcon(instance->iconKey());
        entry.name = instance->name();
        entry.running = instance->isRunning();
        entry.canLaunch = instance->canLaunch();
        entry.details = describeInstance(instance) + separator + (entry.running ? tr("Playing now") : playedAgo(instance->lastLaunch()));
        recent.append(entry);
    }
    m_homePage->setRecentInstances(recent);
}

void MainWindow::populateHomeInstanceMenu()
{
    m_homeInstanceMenu->clear();

    auto instanceList = APPLICATION->instances();
    QList<BaseInstance*> sorted;
    sorted.reserve(instanceList->count());
    for (int i = 0; i < instanceList->count(); i++) {
        if (auto instance = instanceList->at(i))
            sorted.append(instance);
    }
    std::sort(sorted.begin(), sorted.end(),
              [](BaseInstance* a, BaseInstance* b) { return QString::localeAwareCompare(a->name(), b->name()) < 0; });

    if (sorted.isEmpty()) {
        QAction* empty = m_homeInstanceMenu->addAction(tr("No instances yet"));
        empty->setEnabled(false);
    }

    for (BaseInstance* instance : sorted) {
        QString label = instance->name();
        label.replace(QLatin1Char('&'), QStringLiteral("&&"));
        QAction* action = m_homeInstanceMenu->addAction(APPLICATION->icons()->getIcon(instance->iconKey()), label);
        action->setCheckable(true);
        action->setChecked(instance == m_selectedInstance);
        const QString id = instance->id();
        connect(action, &QAction::triggered, this, [this, id] { setSelectedInstanceById(id); });
    }

    m_homeInstanceMenu->addSeparator();
    m_homeInstanceMenu->addAction(ui->actionAddInstance);
    QAction* manage = m_homeInstanceMenu->addAction(tr("Manage instances"));
    connect(manage, &QAction::triggered, this, [this] { switchToPage(Page::Instances); });
}

void MainWindow::updateHomePlayer()
{
    if (!m_homePage)
        return;

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();
    if (!account) {
        m_homePage->showNoPlayer();
        return;
    }

    const QString name = account->profileName().isEmpty() ? account->displayName() : account->profileName();
    const bool isMsa = account->accountType() == AccountType::MSA;

    // XylarSkins stores every skin under the offline UUID of the player name, Microsoft accounts
    // included (their Mojang id is only linked to it), so the cache uses that UUID too.
    QString uuid;
    if (!name.isEmpty())
        uuid = XylarSkinClient::computeOfflineUUID(name);
    // The id the game itself plays with: the Mojang profile id online, the offline UUID otherwise.
    QString gameUuid = uuid;
    if (isMsa) {
        gameUuid = account->profileId();
        if (!gameUuid.contains(QLatin1Char('-')) && gameUuid.length() == 32) {
            gameUuid.insert(8, QLatin1Char('-'));
            gameUuid.insert(13, QLatin1Char('-'));
            gameUuid.insert(18, QLatin1Char('-'));
            gameUuid.insert(23, QLatin1Char('-'));
        }
    }

    QImage skin;
    bool slim = false;
    QString note;

    // Online, a Microsoft account shows its Mojang skin to everyone; XylarSkins is for offline play.
    if (!isMsa && !uuid.isEmpty()) {
        QFile cached(QDir(XylarSkinClient::defaultCacheDir()).filePath(uuid + QStringLiteral(".png")));
        if (cached.open(QIODevice::ReadOnly) && skin.loadFromData(cached.readAll(), "PNG")) {
            slim = SkinPreview::looksSlim(skin);
            note = tr("Your XylarSkins skin. This is what other players see.");
        } else {
            skin = QImage();
        }
    }

    if (skin.isNull() && account->accountData()) {
        const auto& profileSkin = account->accountData()->minecraftProfile.skin;
        if (!profileSkin.data.isEmpty() && skin.loadFromData(profileSkin.data, "PNG")) {
            slim = profileSkin.variant.compare(QStringLiteral("slim"), Qt::CaseInsensitive) == 0 || SkinPreview::looksSlim(skin);
            note = tr("Your Minecraft skin, straight from Mojang.");
        } else {
            skin = QImage();
        }
    }

    // Ask Mojang: by profile UUID for Microsoft accounts, by username for offline ones.
    m_homeSkinKey.clear();
    if (skin.isNull() && m_skinFetcher) {
        const QString key = isMsa ? MojangSkinFetcher::keyForUuid(gameUuid) : MojangSkinFetcher::keyForName(name);
        MojangSkinFetcher::Skin fetched;
        if (m_skinFetcher->cached(key, &fetched) && !fetched.texture.isNull()) {
            skin = fetched.texture;
            slim = fetched.slim || SkinPreview::looksSlim(skin);
            note = isMsa ? tr("Your Minecraft skin, straight from Mojang.")
                         : tr("Skin of the Minecraft profile \"%1\", from Mojang.").arg(name);
        }
        // Refresh in the background (cached results come back immediately).
        m_homeSkinKey = key;
        if (isMsa && !gameUuid.isEmpty())
            m_skinFetcher->fetchByUuid(gameUuid);
        else if (!isMsa && !name.isEmpty())
            m_skinFetcher->fetchByName(name);
    }

    if (skin.isNull() && !gameUuid.isEmpty()) {
        // No custom skin: the game picks one of its default skins from the UUID.
        // The texture is read from a client jar the launcher already downloaded.
        QString preferredVersion;
        if (auto minecraft = dynamic_cast<MinecraftInstance*>(m_selectedInstance)) {
            if (auto profile = minecraft->getPackProfile())
                preferredVersion = profile->getComponentVersion(QStringLiteral("net.minecraft"));
        }
        const DefaultSkin::Result fallback = DefaultSkin::forUuid(gameUuid, preferredVersion);
        if (!fallback.texture.isNull()) {
            skin = fallback.texture;
            slim = fallback.slim;
            note = tr("No custom skin, so the game gives you this default one.");
        } else if (!m_defaultSkinDownloadTried && !DefaultSkin::haveClientJar()) {
            // Fresh install: nothing to read the default skins from yet. Fetch them now
            // instead of showing a placeholder until the first game.
            m_defaultSkinDownloadTried = true;
            DefaultSkin::downloadClientJar(APPLICATION->network(), this, [this](bool ok) {
                if (ok)
                    updateHomePlayer();
            });
            note = tr("Getting your default skin...");
        }
    }

    if (skin.isNull() && note.isEmpty()) {
        note = isMsa ? tr("We couldn't load your skin yet. It will show up after the next sign-in.")
                     : tr("No custom skin yet. Play once and your default game skin shows up here.");
    }

    m_homePage->showPlayer(name, note, skin, slim);
    // The skin may have just changed: show the new face in Social too.
    if (m_socialPanel)
        m_socialPanel->refreshSkin(name);
}

void MainWindow::watchInstancesForPresence()
{
    auto instanceList = APPLICATION->instances();
    for (int i = 0; i < instanceList->count(); i++) {
        if (auto instance = instanceList->at(i))
            connect(instance, &BaseInstance::runningStatusChanged, this, &MainWindow::onInstanceRunningChanged, Qt::UniqueConnection);
    }
}

void MainWindow::onInstanceRunningChanged(bool running)
{
    auto instance = qobject_cast<BaseInstance*>(sender());
    if (!instance)
        return;
    if (running) {
        m_playingInstance = instance;
    } else if (instance == m_playingInstance) {
        // Another game may still be running.
        m_playingInstance = nullptr;
        auto instanceList = APPLICATION->instances();
        for (int i = 0; i < instanceList->count() && !m_playingInstance; i++) {
            if (auto other = instanceList->at(i); other && other != instance && other->isRunning())
                m_playingInstance = other;
        }
    }
    if (m_game)
        m_game->watch(m_playingInstance && m_playingInstance->isRunning() ? m_playingInstance : nullptr);
    updateSocialPlayer();
    updateDiscordPresence();
    updateSocialActivity();
    updateHomeRecent();
    // A game just started or stopped: what the launcher loaded for it can go back to the system.
    QTimer::singleShot(3000, this, [] { MemoryTrim::release(); });
    // The selected instance's skin source (default skin jar) may only exist after a first launch.
    if (!running)
        updateHomePlayer();
}

void MainWindow::showPlayerProfile(const QString& name)
{
    if (!m_profiles || name.isEmpty())
        return;
    auto dialog = new PlayerProfileDialog(m_profiles, name, m_socialPanel ? m_socialPanel->skinFor(name) : QImage(), this);
    if (m_socialPanel) {
        connect(m_socialPanel, &SocialPanel::skinChanged, dialog, [dialog, name](const QString& who, const QImage& skin) {
            if (who.compare(name, Qt::CaseInsensitive) == 0)
                dialog->setSkin(skin);
        });
    }
    dialog->show();
}

void MainWindow::updateDiscordPresence()
{
    if (!m_discord)
        return;
    auto settings = APPLICATION->settings();
    m_discord->setClientId(settings->get("DiscordClientId").toString());
    m_discord->setEnabled(settings->get("DiscordRichPresence").toBool());
    const QString site = settings->get("XylarSiteUrl").toString().trimmed();
    m_discord->setButton(tr("Get XylarJava"), site.startsWith(QLatin1String("https://")) ? site : QString());

    if (m_playingInstance && m_playingInstance->isRunning()) {
        const QString what = describeInstance(m_playingInstance);
        QString state;
        switch (m_game ? m_game->where() : GameActivity::Where::Menus) {
            case GameActivity::Where::Menus: state = tr("In the menus"); break;
            case GameActivity::Where::Singleplayer: state = tr("Singleplayer"); break;
            case GameActivity::Where::Server: state = tr("On %1").arg(gameServerLabel()); break;
        }
        const qint64 started = m_game && m_game->startedAt() > 0 ? m_game->startedAt() : QDateTime::currentSecsSinceEpoch();
        m_discord->show(tr("Playing %1").arg(what.isEmpty() ? m_playingInstance->name() : what), state, started);
    } else {
        m_playingInstance = nullptr;
        const LocalServer* hosting = hostedServer();
        m_discord->show(tr("In the launcher"), hosting ? tr("Hosting %1").arg(hosting->config().name) : tr("Picking what to play"),
                        m_launcherStartedAt);
    }
}

void MainWindow::watchLocalServers()
{
    if (!m_localServers)
        return;
    for (LocalServer* server : m_localServers->servers()) {
        connect(server, &LocalServer::stateChanged, this, &MainWindow::onLocalServerChanged, Qt::UniqueConnection);
        connect(server, &LocalServer::addressesChanged, this, &MainWindow::onLocalServerChanged, Qt::UniqueConnection);
    }
    onLocalServerChanged();
}

void MainWindow::onLocalServerChanged()
{
    if (m_socialPanel) {
        LocalServer* server = hostedServer();
        m_socialPanel->setInviteTarget(server && server->state() == LocalServer::State::Running ? server->config().name : QString());
    }
    updateDiscordPresence();
    updateSocialActivity();
}

LocalServer* MainWindow::hostedServer() const
{
    if (!m_localServers)
        return nullptr;
    for (LocalServer* server : m_localServers->servers()) {
        if (server->isRunning())
            return server;
    }
    return nullptr;
}

QString MainWindow::friendAddress(LocalServer* server) const
{
    if (!server)
        return {};
    const QString playit = server->playitAddress();
    if (!playit.isEmpty())
        return playit;
    return server->publicIp().isEmpty() ? QString() : server->joinAddress(server->publicIp());
}

QString MainWindow::sharedGameServer() const
{
    if (!m_game || m_game->where() != GameActivity::Where::Server)
        return {};
    const QString host = m_game->host();
    if (GameActivity::isLocalHost(host)) {
        // Our own server, joined on this computer: friends use its playit.gg address.
        LocalServer* server = hostedServer();
        return server ? server->playitAddress() : QString();
    }
    // Raw IP addresses stay private: they may be someone's home.
    if (GameActivity::isIpAddress(host))
        return {};
    return m_game->address();
}

QString MainWindow::gameServerLabel() const
{
    if (!m_game)
        return {};
    const QString host = m_game->host();
    LocalServer* server = hostedServer();
    if (server && (GameActivity::isLocalHost(host) || (!server->playitAddress().isEmpty() &&
                                                       server->playitAddress().startsWith(host, Qt::CaseInsensitive))))
        return server->config().name;
    if (GameActivity::isLocalHost(host) || GameActivity::isIpAddress(host))
        return tr("a server");
    return host;
}

void MainWindow::inviteFriend(const QString& name)
{
    QPointer<LocalServer> server = hostedServer();
    if (!server || !m_social)
        return;
    const QString address = friendAddress(server);
    if (address.isEmpty()) {
        QMessageBox::information(this, tr("Invite"),
                                 tr("%1 doesn't have an address friends can reach yet. Turn on playit.gg in its settings, or wait "
                                    "until it shows a public address.")
                                     .arg(server->config().name));
        return;
    }
    // On the whitelist first, so the invite works the moment they click Join.
    server->allowPlayer(name, [this, server, name, address](const QString& error) {
        if (!server || !m_social)
            return;
        if (!error.isEmpty()) {
            QMessageBox::warning(this, tr("Invite"), tr("Couldn't add %1 to the whitelist: %2").arg(name, error));
            return;
        }
        m_social->invite(name, address, server->config().minecraftVersion, server->config().name);
    });
}

void MainWindow::joinServer(const QString& server, const QString& version, const QString& friendName)
{
    if (server.isEmpty())
        return;
    if (m_playingInstance && m_playingInstance->isRunning()) {
        QMessageBox::information(this, tr("Join %1").arg(friendName),
                                 tr("Close the game first, then press Join again. XylarJava will start the right version and connect to "
                                    "%1.")
                                     .arg(server));
        return;
    }

    // An instance of the same Minecraft version, the most recently played first.
    auto instances = APPLICATION->instances();
    auto findInstance = [&](const QSet<QString>& skip) -> BaseInstance* {
        BaseInstance* best = nullptr;
        for (int i = 0; i < instances->count(); i++) {
            auto minecraft = dynamic_cast<MinecraftInstance*>(instances->at(i));
            if (!minecraft || skip.contains(minecraft->id()) || !minecraft->canLaunch() || !minecraft->getPackProfile())
                continue;
            if (!version.isEmpty() && minecraft->getPackProfile()->getComponentVersion(QStringLiteral("net.minecraft")) != version)
                continue;
            if (!best || minecraft->lastLaunch() > best->lastLaunch())
                best = minecraft;
        }
        return best;
    };

    BaseInstance* instance = nullptr;
    if (version.isEmpty())
        instance = m_selectedInstance && m_selectedInstance->canLaunch() ? m_selectedInstance : findInstance({});
    else
        instance = findInstance({});

    if (!instance) {
        if (version.isEmpty())
            return;
        const auto answer = QMessageBox::question(
            this, tr("Join %1").arg(friendName),
            tr("%1 plays Minecraft %2, and you don't have an instance of it yet. Make one and join?").arg(friendName, version),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (answer != QMessageBox::Yes)
            return;
        QSet<QString> before;
        for (int i = 0; i < instances->count(); i++)
            before.insert(instances->at(i)->id());
        auto task = new VanillaCreationTask(version);
        task->setName(tr("Minecraft %1").arg(version));
        task->setIcon(QStringLiteral("default"));
        task->setGroup(APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString());
        instanceFromInstanceTask(task);
        instance = findInstance(before);
        if (!instance)
            return;  // creation failed or was cancelled; the task already said why
    }

    setSelectedInstanceById(instance->id());
    updateHomePage();
    APPLICATION->launch(instance, LaunchMode::Normal, std::make_shared<MinecraftTarget>(MinecraftTarget::parse(server, false)));
}

void MainWindow::showSocialNotice(int kind, const QString& name, const QString& detail)
{
    if (!m_notifier || !APPLICATION->settings()->get("SocialNotifications").toBool())
        return;
    const auto notice = static_cast<SocialClient::Notice>(kind);
    // With the launcher in front and Social open, the panel already shows it.
    const bool watching = isActiveWindow() && m_socialPanel && m_socialPanel->isOpen();
    switch (notice) {
        case SocialClient::Notice::FriendRequest:
            if (!watching)
                m_notifier->notify(tr("Friend request"), tr("%1 wants to be your friend.").arg(name));
            break;
        case SocialClient::Notice::RequestAccepted:
            if (!watching)
                m_notifier->notify(tr("New friend"), tr("%1 accepted your friend request.").arg(name));
            break;
        case SocialClient::Notice::FriendOnline:
            if (!isActiveWindow())
                m_notifier->notify(tr("%1 is online").arg(name), tr("Say hi, or invite them to play."));
            break;
        case SocialClient::Notice::Invite:
            if (!watching)
                m_notifier->notify(tr("%1 invited you").arg(name), tr("Join them on %1 from XylarJava.").arg(detail));
            break;
    }
}

void MainWindow::showSocialPanel()
{
    if (isMinimized())
        showNormal();
    show();
    raise();
    activateWindow();
    switchToPage(Page::Home);
    if (m_socialButton)
        m_socialButton->setChecked(true);
}

void MainWindow::updateKnownPlayers()
{
    // Servers in offline mode learn these names, spelled right, before they start.
    QStringList names;
    auto accounts = APPLICATION->accounts();
    for (int i = 0; i < accounts->count(); i++) {
        if (auto account = accounts->at(i))
            names << (account->profileName().isEmpty() ? account->displayName() : account->profileName());
    }
    if (m_social) {
        for (const auto& person : m_social->friends())
            names << person.name;
        for (const auto& request : m_social->incoming())
            names << request.name;
    }
    LocalServer::setKnownPlayers(names);
}

void MainWindow::updateSocialVisibility()
{
    if (!m_socialButton)
        return;
    // Social belongs to the Home screen only; the other pages get the full width.
    const bool home = m_currentPage == Page::Home;
    m_socialButton->setVisible(home);
    if (m_socialPanel->isOpen() != (home && m_socialButton->isChecked()))
        m_socialPanel->setOpen(home && m_socialButton->isChecked());
}

void MainWindow::updateSocialPlayer()
{
    if (!m_social)
        return;
    // While a game runs, Social is the player in it: picking another account in the launcher
    // changes who plays next, not who is playing now. Names are Minecraft names (offline or Microsoft).
    QString name;
    if (m_playingInstance && m_playingInstance->isRunning())
        name = m_playingInstance->playerName();
    if (name.isEmpty()) {
        MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();
        if (account)
            name = account->profileName().isEmpty() ? account->displayName() : account->profileName();
    }
    m_social->setPlayer(name);
}

void MainWindow::updateSocialActivity()
{
    if (!m_social)
        return;
    if (m_playingInstance && m_playingInstance->isRunning()) {
        QString what = describeInstance(m_playingInstance);
        if (what.isEmpty())
            what = m_playingInstance->name();
        const auto where = m_game ? m_game->where() : GameActivity::Where::Menus;
        if (where == GameActivity::Where::Server)
            what = tr("%1  ·  on %2").arg(what, gameServerLabel());
        else if (where == GameActivity::Where::Singleplayer)
            what = tr("%1  ·  singleplayer").arg(what);
        m_social->setActivity(QStringLiteral("playing"), what, sharedGameServer(), m_game ? m_game->minecraftVersion() : QString());
    } else if (m_away) {
        m_social->setActivity(QStringLiteral("away"));
    } else {
        m_social->setActivity(QStringLiteral("launcher"));
    }
}

// macOS always has a native menu bar, so these fixes are not applicable
// Other systems may or may not have a native menu bar (most do not - it seems like only Ubuntu Unity does)
#ifndef Q_OS_MAC
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    // Alt used to toggle the old menu bar; XylarJava has no menu bar.
    QMainWindow::keyReleaseEvent(event);
}
#endif

void MainWindow::retranslateUi()
{
    if (m_selectedInstance) {
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
    } else {
        m_statusLeft->setText(tr("No instance selected"));
    }

    ui->retranslateUi(this);

    MinecraftAccountPtr defaultAccount = APPLICATION->accounts()->defaultAccount();
    if (defaultAccount) {
        auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
    }

    changeIconButton->setToolTip(ui->actionChangeInstIcon->toolTip());
    renameButton->setToolTip(ui->actionRenameInstance->toolTip());

    if (m_sideBar) {
        m_sideBar->setPageText(static_cast<int>(Page::Home), tr("Home"));
        m_sideBar->setPageText(static_cast<int>(Page::Instances), tr("Instances"));
        m_sideBar->setPageText(static_cast<int>(Page::Servers), tr("Servers"));
        m_sideBar->setPageText(static_cast<int>(Page::Settings), tr("Settings"));
    }
    if (m_addAccountAction)
        m_addAccountAction->setText(tr("Add an account..."));
    updateHomePage();
    updateHomePlayer();
}

MainWindow::~MainWindow() {}

QMenu* MainWindow::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->mainToolBar->toggleViewAction());

    filteredMenu->addAction(ui->actionToggleStatusBar);
    filteredMenu->addAction(ui->actionLockToolbars);

    return filteredMenu;
}
void MainWindow::hideLegacyBars()
{
    // Called again after restoreState(): a window state saved by an older version would
    // otherwise put these bars back.
    removeToolBar(ui->mainToolBar);
    ui->mainToolBar->hide();
    removeToolBar(ui->instanceToolBar);
    ui->instanceToolBar->hide();
    removeToolBar(ui->newsToolBar);
    ui->newsToolBar->hide();
    statusBar()->hide();
}

void MainWindow::setStatusBarVisibility(bool state)
{
    // The status bar never shows in XylarJava (see the constructor); only remember the choice.
    APPLICATION->settings()->set("StatusBarVisible", state);
}
void MainWindow::lockToolbars(bool state)
{
    ui->mainToolBar->setMovable(!state);
    ui->instanceToolBar->setMovable(!state);
    ui->newsToolBar->setMovable(!state);
    APPLICATION->settings()->set("ToolbarsLocked", state);
}

void MainWindow::showInstanceContextMenu(const QPoint& pos)
{
    QList<QAction*> actions;

    QAction* actionSep = new QAction("", this);
    actionSep->setSeparator(true);

    bool onInstance = view->indexAt(pos).isValid();
    if (onInstance) {
        // reuse the file menu actions
        actions = ui->fileMenu->actions();

        // remove the add instance action, launcher settings action and close action
        actions.removeFirst();
        actions.removeLast();
        actions.removeLast();

        // actionChangeInstIcon intentionally omitted: instance icon editing is disabled.
        actions.prepend(ui->actionRenameInstance);

        // Repair: reads the last log and fixes what it finds.
        QAction* actionRepair = new QAction(tr("Repair..."), this);
        actionRepair->setEnabled(!m_selectedInstance->isRunning());
        connect(actionRepair, &QAction::triggered, this, [this] { APPLICATION->repairInstance(m_selectedInstance); });
        actions.append(actionRepair);

        // add header
        actions.prepend(actionSep);
        QAction* actionVoid = new QAction(m_selectedInstance->name(), this);
        actionVoid->setEnabled(false);
        actions.prepend(actionVoid);
    } else {
        auto group = view->groupNameAt(pos);

        QAction* actionVoid = new QAction(group.isNull() ? BuildConfig.LAUNCHER_DISPLAYNAME : group, this);
        actionVoid->setEnabled(false);

        QAction* actionCreateInstance = new QAction(tr("&Create instance"), this);
        actionCreateInstance->setToolTip(ui->actionAddInstance->toolTip());
        if (!group.isNull()) {
            QVariantMap instance_action_data;
            instance_action_data["group"] = group;
            actionCreateInstance->setData(instance_action_data);
        }

        connect(actionCreateInstance, &QAction::triggered, this, &MainWindow::on_actionAddInstance_triggered);

        actions.prepend(actionSep);
        actions.prepend(actionVoid);
        actions.append(actionCreateInstance);
        if (!group.isNull()) {
            QAction* actionDeleteGroup = new QAction(tr("&Delete group"), this);
            connect(actionDeleteGroup, &QAction::triggered, this, [this, group] { deleteGroup(group); });
            actions.append(actionDeleteGroup);

            QAction* actionRenameGroup = new QAction(tr("&Rename group"), this);
            connect(actionRenameGroup, &QAction::triggered, this, [this, group] { renameGroup(group); });
            actions.append(actionRenameGroup);
        }
    }
    QMenu myMenu;
    myMenu.addActions(actions);
    /*
    if (onInstance)
        myMenu.setEnabled(m_selectedInstance->canLaunch());
    */
    myMenu.exec(view->mapToGlobal(pos));
}

void MainWindow::updateMainToolBar()
{
    // The sidebar replaces both the menu bar and the main toolbar.
    ui->menuBar->setVisible(false);
    ui->mainToolBar->setVisible(false);
}

void MainWindow::updateLaunchButton()
{
    QMenu* launchMenu = ui->actionLaunchInstance->menu();
    if (launchMenu)
        launchMenu->clear();
    else
        launchMenu = new QMenu(this);
    if (m_selectedInstance)
        m_selectedInstance->populateLaunchMenu(launchMenu);
    ui->actionLaunchInstance->setMenu(launchMenu);
    if (m_homePage)
        m_homePage->setLaunchMenu(launchMenu);
}

void MainWindow::updateThemeMenu()
{
    QMenu* themeMenu = ui->actionChangeTheme->menu();

    if (themeMenu) {
        themeMenu->clear();
    } else {
        themeMenu = new QMenu(this);
    }

    auto themes = APPLICATION->themeManager()->getValidApplicationThemes();

    QActionGroup* themesGroup = new QActionGroup(this);

    for (auto* theme : themes) {
        QAction* themeAction = themeMenu->addAction(theme->name());

        themeAction->setCheckable(true);
        if (APPLICATION->settings()->get("ApplicationTheme").toString() == theme->id()) {
            themeAction->setChecked(true);
        }
        themeAction->setActionGroup(themesGroup);

        connect(themeAction, &QAction::triggered, [theme]() {
            APPLICATION->themeManager()->setApplicationTheme(theme->id());
            APPLICATION->settings()->set("ApplicationTheme", theme->id());
        });
    }

    ui->actionChangeTheme->setMenu(themeMenu);
}

void MainWindow::repopulateAccountsMenu()
{
    ui->accountsMenu->clear();

    // NOTE: this is done so the accounts button text is not set to the accounts menu title
    QMenu* accountsButtonMenu = ui->actionAccountsButton->menu();
    if (accountsButtonMenu) {
        accountsButtonMenu->clear();
    } else {
        accountsButtonMenu = new QMenu(this);
        ui->actionAccountsButton->setMenu(accountsButtonMenu);
    }

    auto accounts = APPLICATION->accounts();
    MinecraftAccountPtr defaultAccount = accounts->defaultAccount();

    QString active_profileId = "";
    if (defaultAccount) {
        // this can be called before accountMenuButton exists
        if (ui->actionAccountsButton) {
            auto profileLabel = profileInUseFilter(defaultAccount->displayName(), defaultAccount->isInUse());
            ui->actionAccountsButton->setText(profileLabel);
        }
    }

    QActionGroup* accountsGroup = new QActionGroup(this);

    if (accounts->count() <= 0) {
        ui->actionNoAccountsAdded->setEnabled(false);
        ui->accountsMenu->addAction(ui->actionNoAccountsAdded);
    } else {
        // TODO: Nicer way to iterate?
        for (int i = 0; i < accounts->count(); i++) {
            MinecraftAccountPtr account = accounts->at(i);
            auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
            QAction* action = new QAction(profileLabel, this);
            action->setData(i);
            action->setCheckable(true);
            action->setActionGroup(accountsGroup);
            if (defaultAccount == account) {
                action->setChecked(true);
            }

            auto face = account->getFace();
            if (!face.isNull()) {
                action->setIcon(face);
            } else {
                action->setIcon(QIcon::fromTheme("noaccount"));
            }

            const int highestNumberKey = 9;
            if (i < highestNumberKey) {
                action->setShortcut(QKeySequence(tr("Ctrl+%1").arg(i + 1)));
            }

            ui->accountsMenu->addAction(action);
            connect(action, &QAction::triggered, this, &MainWindow::changeActiveAccount);
        }
    }

    ui->accountsMenu->addSeparator();

    ui->actionNoDefaultAccount->setData(-1);
    ui->actionNoDefaultAccount->setChecked(!defaultAccount);
    ui->actionNoDefaultAccount->setActionGroup(accountsGroup);

    ui->accountsMenu->addAction(ui->actionNoDefaultAccount);

    connect(ui->actionNoDefaultAccount, &QAction::triggered, this, &MainWindow::changeActiveAccount);

    ui->accountsMenu->addSeparator();
    if (m_addAccountAction)
        ui->accountsMenu->addAction(m_addAccountAction);
    ui->accountsMenu->addAction(ui->actionManageAccounts);

    accountsButtonMenu->addActions(ui->accountsMenu->actions());
}

void MainWindow::updatesAllowedChanged(bool allowed)
{
    if (!APPLICATION->updaterEnabled()) {
        return;
    }
    ui->actionCheckUpdate->setEnabled(allowed);
}

/*
 * Assumes the sender is a QAction
 */
void MainWindow::changeActiveAccount()
{
    QAction* sAction = (QAction*)sender();

    // Profile's associated Mojang username
    if (sAction->data().typeId() != QMetaType::Int)
        return;

    QVariant action_data = sAction->data();
    bool valid = false;
    int index = action_data.toInt(&valid);
    if (!valid) {
        index = -1;
    }
    auto accounts = APPLICATION->accounts();
    accounts->setDefaultAccount(index == -1 ? nullptr : accounts->at(index));
    defaultAccountChanged();
}

void MainWindow::defaultAccountChanged()
{
    repopulateAccountsMenu();
    updateHomePlayer();

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

    updateKnownPlayers();

    updateSocialPlayer();

    // FIXME: this needs adjustment for MSA
    if (account && account->profileName() != "") {
        auto profileLabel = profileInUseFilter(account->displayName(), account->isInUse());
        ui->actionAccountsButton->setText(profileLabel);
        auto face = account->getFace();
        if (face.isNull()) {
            ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
        } else {
            ui->actionAccountsButton->setIcon(face);
        }
        return;
    }

    // Set the icon to the "no account" icon.
    ui->actionAccountsButton->setIcon(QIcon::fromTheme("noaccount"));
    ui->actionAccountsButton->setText(tr("Accounts"));
}

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == view) {
        if (ev->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(ev);
            switch (keyEvent->key()) {
                    /*
                case Qt::Key_Enter:
                case Qt::Key_Return:
                    activateInstance(m_selectedInstance);
                    return true;
                    */
                case Qt::Key_Delete:
                    on_actionDeleteInstance_triggered();
                    return true;
                case Qt::Key_F5:
                    refreshInstances();
                    return true;
                case Qt::Key_F2:
                    on_actionRenameInstance_triggered();
                    return true;
                default:
                    break;
            }
        }
    }
    return QMainWindow::eventFilter(obj, ev);
}

void MainWindow::updateNewsLabel()
{
    newsLabel->setText(QStringLiteral("XylarJava v0.0.0.4-ALPHA just released!"));
    newsLabel->setEnabled(false);
    ui->actionMoreNews->setVisible(false);
}

QList<int> stringToIntList(const QString& string)
{
    QStringList split = string.split(',', Qt::SkipEmptyParts);
    QList<int> out;
    for (int i = 0; i < split.size(); ++i) {
        out.append(split.at(i).toInt());
    }
    return out;
}
QString intListToString(const QList<int>& list)
{
    QStringList slist;
    for (int i = 0; i < list.size(); ++i) {
        slist.append(QString::number(list.at(i)));
    }
    return slist.join(',');
}

void MainWindow::runModalTask(Task* task)
{
    connect(task, &Task::failed,
            [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
    connect(task, &Task::succeeded, [this, task]() {
        QStringList warnings = task->warnings();
        if (warnings.count()) {
            CustomMessageBox::selectable(this, tr("Warnings"), warnings.join('\n'), QMessageBox::Warning)->show();
        }
    });
    connect(task, &Task::aborted, [this] {
        CustomMessageBox::selectable(this, tr("Task aborted"), tr("The task has been aborted by the user."), QMessageBox::Information)
            ->show();
    });
    ProgressDialog loadDialog(this);
    loadDialog.setSkipButton(true, tr("Abort"));
    loadDialog.execWithTask(task);
}

void MainWindow::instanceFromInstanceTask(InstanceTask* rawTask)
{
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(rawTask));
    runModalTask(task.get());
}

void MainWindow::on_actionCopyInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    CopyInstanceDialog copyInstDlg(m_selectedInstance, this);
    if (!copyInstDlg.exec())
        return;

    auto copyTask = new InstanceCopyTask(m_selectedInstance, copyInstDlg.getChosenOptions());
    copyTask->setName(copyInstDlg.instName());
    copyTask->setGroup(copyInstDlg.instGroup());
    copyTask->setIcon(copyInstDlg.iconKey());
    unique_qobject_ptr<Task> task(APPLICATION->instances()->wrapInstanceTask(copyTask));
    runModalTask(task.get());
}

void MainWindow::addInstance(const QString& url, const QMap<QString, QString>& extra_info)
{
    QString groupName;
    do {
        QObject* obj = sender();
        if (!obj)
            break;
        QAction* action = qobject_cast<QAction*>(obj);
        if (!action)
            break;
        auto map = action->data().toMap();
        if (!map.contains("group"))
            break;
        groupName = map["group"].toString();
    } while (0);

    if (groupName.isEmpty()) {
        groupName = APPLICATION->settings()->get("LastUsedGroupForNewInstance").toString();
    }

    NewInstanceDialog newInstDlg(groupName, url, extra_info, this);
    if (!newInstDlg.exec())
        return;

    APPLICATION->settings()->set("LastUsedGroupForNewInstance", newInstDlg.instGroup());

    InstanceTask* creationTask = newInstDlg.extractTask();
    if (creationTask) {
        instanceFromInstanceTask(creationTask);
    }
}

void MainWindow::on_actionAddInstance_triggered()
{
    addInstance();
}

void MainWindow::processURLs(QList<QUrl> urls)
{
    // NOTE: This loop only processes one dropped file!
    for (auto& url : urls) {
        qDebug() << "Processing" << url;

        // The isLocalFile() check below doesn't work as intended without an explicit scheme.
        if (url.scheme().isEmpty())
            url.setScheme("file");

        ModPlatform::IndexedVersion version;
        QMap<QString, QString> extra_info;
        QUrl local_url;
        if (!url.isLocalFile()) {  // download the remote resource and identify

            const bool isExternalURLImport =
                (url.host().toLower() == "import") ||
                (url.path().startsWith("/import", Qt::CaseInsensitive));

            QUrl dl_url;
            if (url.scheme() == "curseforge" || (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && url.host() == "install")) {
                // need to find the download link for the modpack / resource
                // format of url curseforge://install?addonId=IDHERE&fileId=IDHERE
                // format of url binaryname://install?platform=curseforge&addonId=IDHERE&fileId=IDHERE
                QUrlQuery query(url);
                
                // check if this is a binaryname:// url
                if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME) {
                    // check this is an curseforge platform request
                    if (query.queryItemValue("platform").toLower() != "curseforge") {
                        qDebug() << "Invalid mod distribution platform:" << query.queryItemValue("platform");
                        continue;
                    }
                }

                if (query.allQueryItemValues("addonId").isEmpty() || query.allQueryItemValues("fileId").isEmpty()) {
                    qDebug() << "Invalid curseforge link:" << url;
                    continue;
                }

                auto addonId = query.allQueryItemValues("addonId")[0];
                auto fileId = query.allQueryItemValues("fileId")[0];

                extra_info.insert("pack_id", addonId);
                extra_info.insert("pack_version_id", fileId);

                auto api = FlameAPI();
                auto [job, array] = api.getFile(addonId, fileId);

                connect(job.get(), &Task::failed, this,
                        [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
                connect(job.get(), &Task::succeeded, this, [this, array, addonId, fileId, &dl_url, &version] {
                    qDebug() << "Returned CFURL Json:\n" << array->toStdString().c_str();
                    auto doc = Json::requireDocument(*array);
                    auto data = doc.object()["data"].toObject();
                    // No way to find out if it's a mod or a modpack before here
                    // And also we need to check if it ends with .zip, instead of any better way
                    version = FlameMod::loadIndexedPackVersion(data);
                    auto fileName = version.fileName;

                    // Have to use ensureString then use QUrl to get proper url encoding
                    dl_url = QUrl(version.downloadUrl);
                    if (!dl_url.isValid()) {
                        CustomMessageBox::selectable(
                            this, tr("Error"),
                            tr("The modpack, mod, or resource %1 is blocked for third-parties! Please download it manually.").arg(fileName),
                            QMessageBox::Critical)
                            ->show();
                        return;
                    }

                    QFileInfo dl_file(dl_url.fileName());
                });

                {  // drop stack
                    ProgressDialog dlUrlDialod(this);
                    dlUrlDialod.setSkipButton(true, tr("Abort"));
                    dlUrlDialod.execWithTask(job.get());
                }

            } else if (url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME && !isExternalURLImport) {
                QVariantMap receivedData;
                const QUrlQuery query(url.query());
                const auto items = query.queryItems();
                for (auto it = items.begin(), end = items.end(); it != end; ++it)
                    receivedData.insert(it->first, it->second);
                emit APPLICATION->oauthReplyRecieved(receivedData);
                continue;
            } else if ((url.scheme() == "xylarjava" || url.scheme() == "xylarjava" || url.scheme() == BuildConfig.LAUNCHER_APP_BINARY_NAME)
                        && isExternalURLImport) {
                // Launcher URL protocol modpack import.
                // preferred import format: xylarjava://import?url=ENCODED
                const auto host = url.host().toLower();
                const auto path = url.path();

                QString encodedTarget;

                {
                    QUrlQuery query(url);
                    const auto values = query.allQueryItemValues("url");
                    if (!values.isEmpty()) {
                        encodedTarget = values.first();
                    }
                }

                // alternative import format: xylarjava://import/ENCODED
                if (encodedTarget.isEmpty()) {

                    QString p = path;

                    if (p.startsWith("/import/", Qt::CaseInsensitive)) {
                        p = p.mid(QString("/import/").size());
                    } else if (host == "import" && p.startsWith("/")) {
                        p = p.mid(1);
                    }

                    if (!p.isEmpty() && p != "/import") {
                        encodedTarget = p;
                    }
                }

                if (encodedTarget.isEmpty()) {
                    CustomMessageBox::selectable(
                        this,
                        tr("Error"),
                        tr("Invalid import link: missing 'url' parameter."),
                        QMessageBox::Critical
                    )->show();
                    continue;
                }

                const QString decodedStr = QUrl::fromPercentEncoding(encodedTarget.toUtf8()).trimmed();

                QUrl target = QUrl::fromUserInput(decodedStr);

                // Validate: only allow http(s)
                if (!target.isValid() || (target.scheme() != "https" && target.scheme() != "http")) {
                    CustomMessageBox::selectable(
                        this,
                        tr("Error"),
                        tr("Invalid import link: URL must be http(s)."),
                        QMessageBox::Critical
                    )->show();
                    continue;
                }

                const auto res = QMessageBox::question(
                    this,
                    tr("Install modpack"),
                    tr("Do you want to download and import a modpack from:\n%1\n\nURL:\n%2")
                        .arg(target.host(), target.toString()),
                    QMessageBox::Yes | QMessageBox::No,
                    QMessageBox::Yes
                );
                if (res != QMessageBox::Yes) {
                    continue;
                }

                dl_url = target;
            } else {
                dl_url = url;
            }

            if (!dl_url.isValid()) {
                continue;  // no valid url to download this resource
            }

            const QString path = dl_url.host() + '/' + dl_url.path();
            auto entry = APPLICATION->metacache()->resolveEntry("general", path);
            entry->setStale(true);
            auto dl_job = unique_qobject_ptr<NetJob>(new NetJob(tr("Modpack download"), APPLICATION->network()));
            dl_job->addNetAction(Net::ApiDownload::makeCached(dl_url, entry));
            auto archivePath = entry->getFullPath();

            bool dl_success = false;
            connect(dl_job.get(), &Task::failed, this,
                    [this](QString reason) { CustomMessageBox::selectable(this, tr("Error"), reason, QMessageBox::Critical)->show(); });
            connect(dl_job.get(), &Task::succeeded, this, [&dl_success] { dl_success = true; });

            {  // drop stack
                ProgressDialog dlUrlDialod(this);
                dlUrlDialod.setSkipButton(true, tr("Abort"));
                dlUrlDialod.execWithTask(dl_job.get());
            }

            if (!dl_success) {
                continue;  // no local file to identify
            }
            local_url = QUrl::fromLocalFile(archivePath);

        } else {
            local_url = url;
        }

        auto localFileName = QDir::toNativeSeparators(local_url.toLocalFile());
        QFileInfo localFileInfo(localFileName);

        auto type = ResourceUtils::identify(localFileInfo);

        if (ModPlatform::ResourceTypeUtils::VALID_RESOURCES.count(type) == 0) {  // probably instance/modpack
            addInstance(localFileName, extra_info);
            continue;
        }

        if (APPLICATION->instances()->count() <= 0) {
            CustomMessageBox::selectable(this, tr("No instance!"),
                                         tr("No instance available to add the resource to.\nPlease create a new instance before "
                                            "attempting to install this resource again."),
                                         QMessageBox::Critical)
                ->show();
            continue;
        }
        ImportResourceDialog dlg(localFileName, type, this);

        if (dlg.exec() != QDialog::Accepted)
            continue;

        qDebug() << "Adding resource" << localFileName << "to" << dlg.selectedInstanceKey;

        auto inst = APPLICATION->instances()->getInstanceById(dlg.selectedInstanceKey);
        auto minecraftInst = dynamic_cast<MinecraftInstance*>(inst);

        switch (type) {
            case ModPlatform::ResourceType::ResourcePack:
                minecraftInst->resourcePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::TexturePack:
                minecraftInst->texturePackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::DataPack:
                qWarning() << "Importing of Data Packs not supported at this time. Ignoring" << localFileName;
                break;
            case ModPlatform::ResourceType::Mod:
                minecraftInst->loaderModList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::ShaderPack:
                minecraftInst->shaderPackList()->installResourceWithFlameMetadata(localFileName, version);
                break;
            case ModPlatform::ResourceType::World:
                minecraftInst->worldList()->installWorld(localFileInfo);
                break;
            case ModPlatform::ResourceType::Unknown:
            default:
                qDebug() << "Can't Identify" << localFileName << "Ignoring it.";
                break;
        }
    }
}

void MainWindow::on_actionREDDIT_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.SUBREDDIT_URL));
}

void MainWindow::on_actionDISCORD_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.DISCORD_URL));
}

void MainWindow::on_actionMATRIX_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.MATRIX_URL));
}

void MainWindow::on_actionChangeInstIcon_triggered()
{
    if (!m_selectedInstance)
        return;

    IconPickerDialog dlg(this);
    dlg.execWithSelection(m_selectedInstance->iconKey());
    if (dlg.result() == QDialog::Accepted) {
        m_selectedInstance->setIconKey(dlg.selectedIconKey);
        auto icon = APPLICATION->icons()->getIcon(dlg.selectedIconKey);
        ui->actionChangeInstIcon->setIcon(icon);
        changeIconButton->setIcon(icon);
    }
}

void MainWindow::iconUpdated(QString icon)
{
    if (icon == m_currentInstIcon) {
        auto new_icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
        ui->actionChangeInstIcon->setIcon(new_icon);
        changeIconButton->setIcon(new_icon);
    }
}

void MainWindow::updateInstanceToolIcon(QString new_icon)
{
    m_currentInstIcon = new_icon;
    auto icon = APPLICATION->icons()->getIcon(m_currentInstIcon);
    ui->actionChangeInstIcon->setIcon(icon);
    changeIconButton->setIcon(icon);
}

void MainWindow::setSelectedInstanceById(const QString& id)
{
    if (id.isNull())
        return;
    const QModelIndex index = APPLICATION->instances()->getInstanceIndexById(id);
    if (index.isValid()) {
        QModelIndex selectionIndex = proxymodel->mapFromSource(index);
        view->selectionModel()->setCurrentIndex(selectionIndex, QItemSelectionModel::ClearAndSelect);
        updateStatusCenter();
    }
}

void MainWindow::on_actionChangeInstGroup_triggered()
{
    if (!m_selectedInstance)
        return;

    InstanceId instId = m_selectedInstance->id();
    QString src(APPLICATION->instances()->getInstanceGroup(instId));

    QStringList groups = APPLICATION->instances()->getGroups();
    groups.prepend("");
    int index = groups.indexOf(src);
    bool ok = false;
    QString dst = QInputDialog::getItem(this, tr("Group name"), tr("Enter a new group name."), groups, index, true, &ok);
    dst = dst.simplified();

    if (ok) {
        APPLICATION->instances()->setInstanceGroup(instId, dst);
    }
}

void MainWindow::deleteGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    const int reply = QMessageBox::question(this, tr("Delete group"), tr("Are you sure you want to delete the group '%1'?").arg(group),
                                            QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes)
        APPLICATION->instances()->deleteGroup(group);
}

void MainWindow::renameGroup(QString group)
{
    Q_ASSERT(!group.isEmpty());

    QString name = QInputDialog::getText(this, tr("Rename group"), tr("Enter a new group name."), QLineEdit::Normal, group);
    name = name.simplified();
    if (name.isNull() || name == group)
        return;

    const bool empty = name.isEmpty();
    const bool duplicate = APPLICATION->instances()->getGroups().contains(name, Qt::CaseInsensitive) && group.toLower() != name.toLower();

    if (empty || duplicate) {
        QMessageBox::warning(this, tr("Cannot rename group"), empty ? tr("Cannot set empty name.") : tr("Group already exists. :/"));
        return;
    }

    APPLICATION->instances()->renameGroup(group, name);
}

void MainWindow::undoTrashInstance()
{
    if (!APPLICATION->instances()->undoTrashInstance())
        QMessageBox::warning(
            this, tr("Failed to undo trashing instance"),
            tr("Some instances and shortcuts could not be restored.\nPlease check your trashbin to manually restore them."));
    ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
}

void MainWindow::on_actionViewLauncherRootFolder_triggered()
{
    DesktopServices::openPath(".");
}

void MainWindow::on_actionViewInstanceFolder_triggered()
{
    QString str = APPLICATION->settings()->get("InstanceDir").toString();
    DesktopServices::openPath(str);
}

void MainWindow::on_actionViewCentralModsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("CentralModsDir").toString(), true);
}

void MainWindow::on_actionViewSkinsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->settings()->get("SkinsDir").toString(), true);
}

void MainWindow::on_actionViewIconThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getIconThemesFolder().path(), true);
}

void MainWindow::on_actionViewWidgetThemeFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->themeManager()->getApplicationThemesFolder().path(), true);
}

void MainWindow::on_actionViewIconsFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->icons()->getDirectory(), true);
}

void MainWindow::on_actionViewLogsFolder_triggered()
{
    DesktopServices::openPath("logs", true);
}

void MainWindow::on_actionViewJavaFolder_triggered()
{
    DesktopServices::openPath(APPLICATION->javaPath(), true);
}

void MainWindow::refreshInstances()
{
    APPLICATION->instances()->loadList();
}

void MainWindow::checkForUpdates()
{
    if (APPLICATION->updaterEnabled()) {
        APPLICATION->triggerUpdateCheck();
    } else {
        qWarning() << "Updater not set up. Cannot check for updates.";
    }
}

void MainWindow::on_actionSettings_triggered()
{
    openSettings(QStringLiteral("global-settings"));
}

void MainWindow::globalSettingsClosed()
{
    // FIXME: quick HACK to make this work. improve, optimize.
    APPLICATION->instances()->loadList();
    proxymodel->invalidate();
    proxymodel->sort(0);
    updateMainToolBar();
    updateLaunchButton();
    updateThemeMenu();
    updateStatusCenter();
    updateHomePlayer();
    updateDiscordPresence();
    // This needs to be done to prevent UI elements disappearing in the event the config is changed
    // but PolyMC exits abnormally, causing the window state to never be saved:
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    update();
}

void MainWindow::on_actionEditInstance_triggered()
{
    if (!m_selectedInstance)
        return;

    if (m_selectedInstance->canEdit()) {
        APPLICATION->showInstanceWindow(m_selectedInstance);
    } else {
        CustomMessageBox::selectable(this, tr("Instance not editable"),
                                     tr("This instance is not editable. It may be broken, invalid, or too old. Check logs for details."),
                                     QMessageBox::Critical)
            ->show();
    }
}

void MainWindow::on_actionManageAccounts_triggered()
{
    openSettings(QStringLiteral("accounts"));
}

void MainWindow::on_actionReportBug_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.BUG_TRACKER_URL));
}

void MainWindow::on_actionClearMetadata_triggered()
{
    // This if contains side effects!
    if (!APPLICATION->metacache()->evictAll()) {
        CustomMessageBox::selectable(this, tr("Error"),
                                     tr("Metadata cache clear Failed!\nTo clear the metadata cache manually, press Folders -> View "
                                        "Launcher Root Folder, and after closing the launcher delete the folder named \"meta\"\n"),
                                     QMessageBox::Warning)
            ->show();
    }

    APPLICATION->metacache()->SaveNow();
}

#ifdef Q_OS_MAC
void MainWindow::on_actionAddToPATH_triggered()
{
    auto binaryPath = APPLICATION->applicationFilePath();
    auto targetPath = QString("/usr/local/bin/%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    qDebug() << "Symlinking" << binaryPath << "to" << targetPath;

    QStringList args;
    args << "-e";
    args << QString("do shell script \"mkdir -p /usr/local/bin && ln -sf '%1' '%2'\" with administrator privileges")
                .arg(binaryPath, targetPath);
    auto outcome = QProcess::execute("/usr/bin/osascript", args);
    if (!outcome) {
        QMessageBox::information(this, tr("Successfully added %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                 tr("%1 was successfully added to your PATH. You can now start it by running `%2`.")
                                     .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.LAUNCHER_APP_BINARY_NAME));
    } else {
        QMessageBox::critical(this, tr("Failed to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                              tr("An error occurred while trying to add %1 to PATH").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    }
}
#endif

void MainWindow::on_actionOpenWiki_triggered()
{
    DesktopServices::openUrl(QUrl(BuildConfig.WIKI_URL));
}

void MainWindow::on_actionMoreNews_triggered()
{
}

void MainWindow::newsButtonClicked()
{
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_actionDeleteInstance_triggered()
{
    if (!m_selectedInstance) {
        return;
    }

    if (m_selectedInstance->isRunning()) {
        CustomMessageBox::selectable(this, tr("Cannot Delete Running Instance"),
                                     tr("The selected instance is currently running and cannot be deleted. Please stop the instance before "
                                        "attempting to delete it."),
                                     QMessageBox::Warning, QMessageBox::Ok)
            ->exec();
        return;
    }
    auto id = m_selectedInstance->id();

    QString shortcutStr;
    auto shortcuts = m_selectedInstance->shortcuts();
    if (!shortcuts.isEmpty())
        shortcutStr = tr(" and its %n registered shortcut(s)", "", shortcuts.size());
    auto response = CustomMessageBox::selectable(this, tr("Confirm Deletion"),
                                                 tr("You are about to delete \"%1\"%2.\n"
                                                    "This may be permanent and will completely delete the instance.\n\n"
                                                    "Are you sure?")
                                                     .arg(m_selectedInstance->name(), shortcutStr),
                                                 QMessageBox::Warning, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();

    if (response != QMessageBox::Yes)
        return;

    if (!checkLinkedInstances(id, this, tr("Deleting")))
        return;

    if (APPLICATION->instances()->trashInstance(id)) {
        ui->actionUndoTrashInstance->setEnabled(APPLICATION->instances()->trashedSomething());
    } else {
        APPLICATION->instances()->deleteInstance(id);
    }
    APPLICATION->settings()->set("SelectedInstance", QString());
    selectionBad();
}

void MainWindow::on_actionExportInstanceZip_triggered()
{
    if (m_selectedInstance) {
        ExportInstanceDialog dlg(m_selectedInstance, this);
        dlg.exec();
    }
}

void MainWindow::on_actionExportInstanceMrPack_triggered()
{
    if (m_selectedInstance) {
        auto instance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
        if (instance != nullptr) {
            ExportPackDialog dlg(instance, this);
            dlg.exec();
        }
    }
}

void MainWindow::on_actionExportInstanceFlamePack_triggered()
{
    if (m_selectedInstance) {
        auto instance = dynamic_cast<MinecraftInstance*>(m_selectedInstance);
        if (instance) {
            if (auto cmp = instance->getPackProfile()->getComponent("net.minecraft");
                cmp && cmp->getVersionFile() && cmp->getVersionFile()->type == "snapshot") {
                QMessageBox msgBox(this);
                msgBox.setText("Snapshots are currently not supported by CurseForge modpacks.");
                msgBox.exec();
                return;
            }
            ExportPackDialog dlg(instance, this, ModPlatform::ResourceProvider::FLAME);
            dlg.exec();
        }
    }
}

void MainWindow::on_actionRenameInstance_triggered()
{
    if (m_selectedInstance) {
        view->edit(view->currentIndex());
    }
}

void MainWindow::on_actionViewSelectedInstFolder_triggered()
{
    if (m_selectedInstance) {
        QString str = m_selectedInstance->instanceRoot();
        DesktopServices::openPath(QFileInfo(str));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_localServers && m_localServers->anyRunning()) {
        const auto answer = QMessageBox::question(this, tr("Servers are running"),
                                                  tr("Your servers stop when %1 closes. Players will be disconnected and the worlds "
                                                     "are saved first. Close anyway?")
                                                      .arg(BuildConfig.LAUNCHER_DISPLAYNAME),
                                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            event->ignore();
            return;
        }
    }
    if (m_settingsContainer && !closeSettings()) {
        event->ignore();
        return;
    }
    if (m_social)
        m_social->goOfflineNow();
    // Save the window state and geometry.
    APPLICATION->settings()->set("MainWindowState", QString::fromUtf8(saveState().toBase64()));
    APPLICATION->settings()->set("MainWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    instanceToolbarSetting->set(QString::fromUtf8(ui->instanceToolBar->getVisibilityState().toBase64()));
    event->accept();
    emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
    } else if (event->type() == QEvent::PaletteChange) {
        applyXylarBranding();
    }
    QMainWindow::changeEvent(event);
}

void MainWindow::instanceActivated(QModelIndex index)
{
    if (!index.isValid())
        return;
    QString id = index.data(InstanceList::InstanceIDRole).toString();
    BaseInstance* inst = APPLICATION->instances()->getInstanceById(id);
    if (!inst)
        return;

    activateInstance(inst);
}

void MainWindow::on_actionLaunchInstance_triggered()
{
    if (m_selectedInstance && !m_selectedInstance->isRunning()) {
        APPLICATION->launch(m_selectedInstance);
    }
}

void MainWindow::activateInstance(BaseInstance* instance)
{
    APPLICATION->launch(instance);
}

void MainWindow::on_actionKillInstance_triggered()
{
    if (m_selectedInstance && m_selectedInstance->isRunning()) {
        APPLICATION->kill(m_selectedInstance);
    }
}

void MainWindow::on_actionCreateInstanceShortcut_triggered()
{
    if (!m_selectedInstance)
        return;

    CreateShortcutDialog shortcutDlg(m_selectedInstance, this);
    if (!shortcutDlg.exec())
        return;
    shortcutDlg.createShortcut();
}

void MainWindow::taskEnd()
{
    QObject* sender = QObject::sender();
    if (sender == m_versionLoadTask)
        m_versionLoadTask = NULL;

    sender->deleteLater();
}

void MainWindow::startTask(Task* task)
{
    connect(task, &Task::succeeded, this, &MainWindow::taskEnd);
    connect(task, &Task::failed, this, &MainWindow::taskEnd);
    task->start();
}

void MainWindow::instanceChanged(const QModelIndex& current, [[maybe_unused]] const QModelIndex& previous)
{
    if (!current.isValid()) {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
    if (m_selectedInstance) {
        disconnect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        disconnect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);
    }
    QString id = current.data(InstanceList::InstanceIDRole).toString();
    m_selectedInstance = APPLICATION->instances()->getInstanceById(id);
    if (m_selectedInstance) {
        ui->instanceToolBar->setEnabled(true);
        setInstanceActionsEnabled(true);
        ui->actionLaunchInstance->setEnabled(m_selectedInstance->canLaunch());

        ui->actionKillInstance->setEnabled(m_selectedInstance->isRunning());
        ui->actionExportInstance->setEnabled(m_selectedInstance->canExport());
        renameButton->setText(m_selectedInstance->name());
        m_statusLeft->setText(m_selectedInstance->getStatusbarDescription());
        updateStatusCenter();
        updateInstanceToolIcon(m_selectedInstance->iconKey());

        updateLaunchButton();

        APPLICATION->settings()->set("SelectedInstance", m_selectedInstance->id());

        connect(m_selectedInstance, &BaseInstance::runningStatusChanged, this, &MainWindow::refreshCurrentInstance);
        connect(m_selectedInstance, &BaseInstance::profilerChanged, this, &MainWindow::refreshCurrentInstance);

        updateHomePage();
    } else {
        APPLICATION->settings()->set("SelectedInstance", QString());
        selectionBad();
        return;
    }
}

void MainWindow::instanceSelectRequest(QString id)
{
    setSelectedInstanceById(id);
}

void MainWindow::instanceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    auto current = view->selectionModel()->currentIndex();
    QItemSelection test(topLeft, bottomRight);
    if (test.contains(current)) {
        instanceChanged(current, current);
    }
}

void MainWindow::selectionBad()
{
    // start by reseting everything...
    m_selectedInstance = nullptr;
    m_statusLeft->setText(tr("No instance selected"));

    statusBar()->clearMessage();
    ui->instanceToolBar->setEnabled(false);
    setInstanceActionsEnabled(false);
    updateLaunchButton();
    renameButton->setText(tr("Rename Instance"));
    updateInstanceToolIcon("grass");
    updateHomePage();

    // ...and then see if we can enable the previously selected instance
    setSelectedInstanceById(APPLICATION->settings()->get("SelectedInstance").toString());
}

void MainWindow::checkInstancePathForProblems()
{
    QString instanceFolder = APPLICATION->settings()->get("InstanceDir").toString();
    if (FS::checkProblemticPathJava(QDir(instanceFolder))) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'!\' and this is known to cause Java problems!"));
        warning.setInformativeText(tr("You have now two options: <br/>"
                                      " - change the instance folder in the settings <br/>"
                                      " - move this installation of %1 to a different folder")
                                       .arg(BuildConfig.LAUNCHER_DISPLAYNAME));
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
    auto tempFolderText =
        tr("This is a problem: <br/>"
           " - The launcher will likely be deleted without warning by the operating system <br/>"
           " - close the launcher now and extract it to a real location, not a temporary folder");
    QString pathfoldername = QDir(instanceFolder).absolutePath();
    if (pathfoldername.contains("Rar$", Qt::CaseInsensitive)) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder contains \'Rar$\' - that means you haven't extracted the launcher archive!"));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    } else if (pathfoldername.startsWith(QDir::tempPath()) || pathfoldername.contains("/TempState/")) {
        QMessageBox warning(this);
        warning.setText(tr("Your instance folder is in a temporary folder: \'%1\'!").arg(QDir::tempPath()));
        warning.setInformativeText(tempFolderText);
        warning.setDefaultButton(QMessageBox::Ok);
        warning.exec();
    }
}

void MainWindow::updateStatusCenter()
{
    m_statusCenter->setVisible(APPLICATION->settings()->get("ShowGlobalGameTime").toBool());

    int timePlayed = APPLICATION->instances()->getTotalPlayTime();
    if (timePlayed > 0) {
        m_statusCenter->setText(
            tr("Total playtime: %1")
                .arg(Time::prettifyDuration(timePlayed, APPLICATION->settings()->get("ShowGameTimeWithoutDays").toBool())));
    }
}
// "Instance actions" are actions that require an instance to be selected (i.e. "new instance" is not here)
// Actions that also require other conditions (e.g. a running instance) won't be changed.
void MainWindow::setInstanceActionsEnabled(bool enabled)
{
    ui->actionEditInstance->setEnabled(enabled);
    ui->actionChangeInstGroup->setEnabled(enabled);
    ui->actionViewSelectedInstFolder->setEnabled(enabled);
    ui->actionExportInstance->setEnabled(enabled);
    ui->actionDeleteInstance->setEnabled(enabled);
    ui->actionCopyInstance->setEnabled(enabled);
    ui->actionCreateInstanceShortcut->setEnabled(enabled);
}

void MainWindow::refreshCurrentInstance()
{
    auto current = view->selectionModel()->currentIndex();
    instanceChanged(current, current);
}
