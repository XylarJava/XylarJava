#pragma once

#include <QDialog>
#include <QItemSelection>
#include <QLabel>
#include <QPixmap>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/SkinList.h"
#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"

class Skin3DPreview;

namespace Ui {
class SkinManageDialog;
}
class SkinManageDialog : public QDialog, public SkinProvider {
    Q_OBJECT
   public:
    explicit SkinManageDialog(QWidget* parent, MinecraftAccountPtr acct);
    virtual ~SkinManageDialog();
    void resizeEvent(QResizeEvent* event) override;

    virtual SkinModel* getSelectedSkin() override;
    virtual QHash<QString, QImage> capes() override;

   public slots:
    void selectionChanged(QItemSelection, QItemSelection);
    void activated(QModelIndex);
    void delayed_scroll(QModelIndex);
    void on_openDirBtn_clicked();
    void on_fileBtn_clicked();
    void on_urlBtn_clicked();
    void on_userBtn_clicked();
    void accept() override;
    void on_capeCombo_currentIndexChanged(int index);
    void on_steveBtn_toggled(bool checked);
    void on_resetBtn_clicked();
    void show_context_menu(const QPoint& pos);
    bool eventFilter(QObject* obj, QEvent* ev) override;
    void on_action_Rename_Skin_triggered(bool checked);
    void on_action_Delete_Skin_triggered(bool checked);

   private:
    void setupCapes();

   private:
    MinecraftAccountPtr m_acct;
    Ui::SkinManageDialog* m_ui;
    SkinList m_list;
    QString m_selectedSkinKey;
    QHash<QString, QImage> m_capes;
    QHash<QString, int> m_capesIdx;
    SkinOpenGLWindow* m_skinPreview = nullptr;
    Skin3DPreview* m_softwarePreview = nullptr;
};
