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

   private:
    void retranslateUi();

    /// Apply XylarJava's accent styling to the toolbars and instance view.
    void applyXylarBranding();

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

    BaseInstance* m_selectedInstance = nullptr;
    QString m_currentInstIcon;

    // managed by the application object
    Task* m_versionLoadTask = nullptr;
};
