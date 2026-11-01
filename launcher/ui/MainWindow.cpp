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
#include <QStackedWidget>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>
#include <QWidgetAction>
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
#include <net/ApiDownload.h>
#include <net/NetJob.h>
#include <tools/BaseProfiler.h>
#include <updater/ExternalUpdater.h>
#include "InstanceWindow.h"

#include "ui/GuiUtil.h"
#include "ui/ViewLogWindow.h"
#include "ui/dialogs/AboutDialog.h"
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
#include "ui/widgets/HomePage.h"
#include "ui/widgets/LabeledToolButton.h"

#include "minecraft/PackProfile.h"
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
    ui->viewMenu->addAction(ui->instanceToolBar->toggleViewAction());
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
        m_pages = new QStackedWidget(ui->centralWidget);
        m_pages->setObjectName(QStringLiteral("mainPages"));

        m_homePage = new HomePage(m_pages);
        m_pages->addWidget(m_homePage);

        auto instancesPage = new QWidget(m_pages);
        instancesPage->setObjectName(QStringLiteral("instancesPage"));
        auto instancesLayout = new QVBoxLayout(instancesPage);
        instancesLayout->setContentsMargins(0, 0, 0, 0);
        instancesLayout->setSpacing(0);
        instancesLayout->addWidget(view);
        m_pages->addWidget(instancesPage);

        ui->horizontalLayout->addWidget(m_pages);

        m_homeInstanceMenu = new QMenu(this);
        m_homeInstanceMenu->setStyleSheet(QStringLiteral("QMenu { menu-scrollable: 1; }"));
        connect(m_homeInstanceMenu, &QMenu::aboutToShow, this, &MainWindow::populateHomeInstanceMenu);
        m_homePage->setInstanceMenu(m_homeInstanceMenu);

        connect(m_homePage, &HomePage::playRequested, this, &MainWindow::on_actionLaunchInstance_triggered);
        connect(m_homePage, &HomePage::stopRequested, this, &MainWindow::on_actionKillInstance_triggered);
        connect(m_homePage, &HomePage::createInstanceRequested, this, &MainWindow::on_actionAddInstance_triggered);
        connect(m_homePage, &HomePage::manageInstancesRequested, this, [this] { switchToPage(Page::Instances); });

        // Keep the Home screen in sync when instances are added, removed or reloaded.
        connect(APPLICATION->instances(), &InstanceList::rowsInserted, this, [this] { updateHomePage(); });
        connect(APPLICATION->instances(), &InstanceList::rowsRemoved, this, [this] { updateHomePage(); });
        connect(APPLICATION->instances(), &InstanceList::modelReset, this, [this] { updateHomePage(); });
    }

    // Togglable status bar
    {
        bool statusBarVisible = APPLICATION->settings()->get("StatusBarVisible").toBool();
        ui->actionToggleStatusBar->setChecked(statusBarVisible);
        connect(ui->actionToggleStatusBar, &QAction::toggled, this, &MainWindow::setStatusBarVisibility);
        setStatusBarVisibility(statusBarVisible);
    }

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

    // Top navigation: Home / Instances, in front of the existing toolbar actions
    {
        auto navGroup = new QActionGroup(this);
        navGroup->setExclusive(true);

        m_navHomeAction = new QAction(this);
        m_navHomeAction->setObjectName(QStringLiteral("actionNavHome"));
        m_navHomeAction->setCheckable(true);
        m_navHomeAction->setChecked(true);
        m_navHomeAction->setActionGroup(navGroup);
        connect(m_navHomeAction, &QAction::triggered, this, [this] { switchToPage(Page::Home); });

        m_navInstancesAction = new QAction(this);
        m_navInstancesAction->setObjectName(QStringLiteral("actionNavInstances"));
        m_navInstancesAction->setCheckable(true);
        m_navInstancesAction->setActionGroup(navGroup);
        connect(m_navInstancesAction, &QAction::triggered, this, [this] { switchToPage(Page::Instances); });

        ui->mainToolBar->insertAction(ui->actionAddInstance, m_navHomeAction);
        ui->mainToolBar->insertAction(ui->actionAddInstance, m_navInstancesAction);
        ui->mainToolBar->insertSeparator(ui->actionAddInstance);

        if (auto homeButton = qobject_cast<QToolButton*>(ui->mainToolBar->widgetForAction(m_navHomeAction))) {
            homeButton->setObjectName(QStringLiteral("navButton"));
            homeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
        if (auto instancesButton = qobject_cast<QToolButton*>(ui->mainToolBar->widgetForAction(m_navInstancesAction))) {
            instancesButton->setObjectName(QStringLiteral("navButton"));
            instancesButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }

        // Remember whether the user wants the instance toolbar; it is only shown on the Instances page.
        connect(ui->instanceToolBar->toggleViewAction(), &QAction::triggered, this, [this](bool checked) {
            m_instanceToolBarWanted = checked;
            applyPageChrome();
        });
    }

    // "Add account" entry for the accounts menu
    m_addAccountAction = new QAction(this);
    m_addAccountAction->setIcon(QIcon::fromTheme(QStringLiteral("accounts")));
    connect(m_addAccountAction, &QAction::triggered, this, &MainWindow::on_actionManageAccounts_triggered);

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
    // Theme-aware styling for the main window chrome. Everything is expressed
    // through palette() roles so it follows the active built-in or community theme.
    const QString style = QStringLiteral(
        // Top navigation bar: roomy and flat.
        "QToolBar#mainToolBar { padding: 6px 12px; spacing: 6px; border: none;"
        " border-bottom: 1px solid palette(mid); background: palette(window); }"
        "QToolBar::separator { background: palette(mid); width: 1px; height: 1px; margin: 6px 8px; }"
        "QToolBar QToolButton { border-radius: 8px; padding: 6px 10px; }"
        "QToolBar QToolButton:hover { background-color: palette(button); }"
        "QToolBar QToolButton:pressed { background-color: palette(highlight); color: palette(highlighted-text); }"
        // Navigation tabs: bold text with an accent underline when active.
        "QToolButton#navButton { font-weight: 700; padding: 6px 14px; border-radius: 0;"
        " border-bottom: 2px solid transparent; }"
        "QToolButton#navButton:hover { background: transparent; border-bottom-color: palette(mid); }"
        "QToolButton#navButton:checked { background: transparent; color: palette(highlight);"
        " border-bottom-color: palette(highlight); }"
        // Instance toolbar keeps regular rounded buttons.
        "QToolBar#instanceToolBar { padding: 6px; spacing: 4px; border: none; border-left: 1px solid palette(mid); }"
        "QToolBar#instanceToolBar QToolButton:checked { background-color: palette(highlight); color: palette(highlighted-text); }"
        "QToolBar#newsToolBar { padding: 2px 12px; border: none; }"
        // Status bar sits a touch quieter than the main surface.
        "QStatusBar QLabel { padding: 0 6px; }");

    // Apply only to the toolbars and status bar so the rest of the application
    // (settings pages, dialogs and the custom instance view) keeps its own look.
    ui->mainToolBar->setStyleSheet(style);
    ui->instanceToolBar->setStyleSheet(style);
    ui->newsToolBar->setStyleSheet(style);
    statusBar()->setStyleSheet(style);
}

void MainWindow::showEvent(QShowEvent* event)
{
    QMainWindow::showEvent(event);
    if (!m_pageChromeReady) {
        // The saved window state has been restored by now, so the instance
        // toolbar's visibility is the user's own choice.
        m_instanceToolBarWanted = !ui->instanceToolBar->isHidden();
        m_pageChromeReady = true;
        applyPageChrome();
    }
}

void MainWindow::switchToPage(Page page)
{
    m_currentPage = page;
    if (m_pages)
        m_pages->setCurrentIndex(static_cast<int>(page));
    if (m_navHomeAction)
        m_navHomeAction->setChecked(page == Page::Home);
    if (m_navInstancesAction)
        m_navInstancesAction->setChecked(page == Page::Instances);
    applyPageChrome();
    if (page == Page::Instances && view)
        view->setFocus();
}

void MainWindow::applyPageChrome()
{
    if (!m_pageChromeReady)
        return;
    ui->instanceToolBar->setVisible(m_currentPage == Page::Instances && m_instanceToolBarWanted);
}

QString MainWindow::serializedWindowState()
{
    if (!m_pageChromeReady)
        return QString::fromUtf8(saveState().toBase64());

    const bool shownNow = !ui->instanceToolBar->isHidden();
    ui->instanceToolBar->setVisible(m_instanceToolBarWanted);
    const QString state = QString::fromUtf8(saveState().toBase64());
    ui->instanceToolBar->setVisible(shownNow);
    return state;
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
                    { "com.mumfrey.liteloader", "LiteLoader" },
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

    if (!m_selectedInstance) {
        m_homePage->showNoInstance(APPLICATION->instances()->count() > 0);
        return;
    }

    m_homePage->showInstance(APPLICATION->icons()->getIcon(m_selectedInstance->iconKey()), m_selectedInstance->name(),
                             describeInstance(m_selectedInstance), m_selectedInstance->canLaunch(), m_selectedInstance->isRunning());
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

// macOS always has a native menu bar, so these fixes are not applicable
// Other systems may or may not have a native menu bar (most do not - it seems like only Ubuntu Unity does)
#ifndef Q_OS_MAC
void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Alt && !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool())
        ui->menuBar->setVisible(!ui->menuBar->isVisible());
    else
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

    if (m_navHomeAction)
        m_navHomeAction->setText(tr("Home"));
    if (m_navInstancesAction)
        m_navInstancesAction->setText(tr("Instances"));
    if (m_addAccountAction)
        m_addAccountAction->setText(tr("Add Account..."));
    updateHomePage();
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
void MainWindow::setStatusBarVisibility(bool state)
{
    statusBar()->setVisible(state);
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
    ui->menuBar->setVisible(APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool());
    ui->mainToolBar->setVisible(ui->menuBar->isNativeMenuBar() || !APPLICATION->settings()->get("MenuBarInsteadOfToolBar").toBool());
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

    MinecraftAccountPtr account = APPLICATION->accounts()->defaultAccount();

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
    APPLICATION->ShowGlobalSettings(this, "global-settings");
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
    // This needs to be done to prevent UI elements disappearing in the event the config is changed
    // but PolyMC exits abnormally, causing the window state to never be saved:
    APPLICATION->settings()->set("MainWindowState", serializedWindowState());
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
    APPLICATION->ShowGlobalSettings(this, "accounts");
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
    // Save the window state and geometry.
    APPLICATION->settings()->set("MainWindowState", serializedWindowState());
    APPLICATION->settings()->set("MainWindowGeometry", QString::fromUtf8(saveGeometry().toBase64()));
    instanceToolbarSetting->set(QString::fromUtf8(ui->instanceToolBar->getVisibilityState().toBase64()));
    event->accept();
    emit isClosing();
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslateUi();
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
