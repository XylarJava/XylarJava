#pragma once

#include <QMainWindow>

#include <LoggedProcess.h>
#include "minecraft/MinecraftInstance.h"
#include "ui/pages/BasePage.h"

#include "settings/Setting.h"

class WorldList;
namespace Ui {
class WorldListPage;
}

class WorldListPage : public QMainWindow, public BasePage {
    Q_OBJECT

   public:
    explicit WorldListPage(MinecraftInstance* inst, WorldList* worlds, QWidget* parent = 0);
    virtual ~WorldListPage();

    virtual QString displayName() const override { return tr("Worlds"); }
    virtual QIcon icon() const override { return QIcon::fromTheme("worlds"); }
    virtual QString id() const override { return "worlds"; }
    virtual QString helpPage() const override { return "Worlds"; }
    virtual bool shouldDisplay() const override;
    void retranslate() override;

    virtual void openedImpl() override;
    virtual void closedImpl() override;

   protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;
    bool worldListFilter(QKeyEvent* ev);
    QMenu* createPopupMenu() override;

   protected:
    MinecraftInstance* m_inst;

   private:
    QModelIndex getSelectedWorld();
    bool isWorldSafe(QModelIndex index);
    bool worldSafetyNagQuestion(const QString& actionType);
    void mceditError();

   private:
    Ui::WorldListPage* ui;
    WorldList* m_worlds;
    unique_qobject_ptr<LoggedProcess> m_mceditProcess;
    bool m_mceditStarting = false;

    std::shared_ptr<Setting> m_wide_bar_setting = nullptr;
    std::unique_ptr<DataPackFolderModel> m_datapackModel;

   private slots:
    void on_actionCopy_Seed_triggered();
    void on_actionMCEdit_triggered();
    void on_actionRemove_triggered();
    void on_actionAdd_triggered();
    void on_actionCopy_triggered();
    void on_actionRename_triggered();
    void on_actionRefresh_triggered();
    void on_actionView_Folder_triggered();
    void on_actionData_Packs_triggered();
    void on_actionReset_Icon_triggered();
    void worldChanged(const QModelIndex& current, const QModelIndex& previous);
    void mceditState(LoggedProcess::State state);
    void on_actionJoin_triggered();

    void ShowContextMenu(const QPoint& pos);
};
