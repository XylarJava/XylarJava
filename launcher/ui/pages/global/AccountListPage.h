#pragma once

#include <QMainWindow>
#include <memory>

#include "ui/pages/BasePage.h"

#include "minecraft/auth/AccountList.h"

namespace Ui {
class AccountListPage;
}

class AuthenticateTask;

class AccountListPage : public QMainWindow, public BasePage {
    Q_OBJECT
   public:
    explicit AccountListPage(QWidget* parent = 0);
    ~AccountListPage();

    QString displayName() const override { return tr("Accounts"); }
    QIcon icon() const override
    {
        auto icon = QIcon::fromTheme("accounts");
        if (icon.isNull()) {
            icon = QIcon::fromTheme("noaccount");
        }
        return icon;
    }
    QString id() const override { return "accounts"; }
    QString helpPage() const override { return "getting-started/adding-an-account"; }
    void retranslate() override;

   public slots:
    void on_actionAddMicrosoft_triggered();
    void on_actionAddOffline_triggered();
    void on_actionRemove_triggered();
    void on_actionRefresh_triggered();
    void on_actionSetDefault_triggered();
    void on_actionNoDefault_triggered();
    void on_actionManageSkins_triggered();
    void on_actionXylarSkins_triggered();
    void on_actionMoveUp_triggered();
    void on_actionMoveDown_triggered();

    void listChanged();

    //! Updates the states of the dialog's buttons.
    void updateButtonStates();

   protected slots:
    void ShowContextMenu(const QPoint& pos);

   private:
    void changeEvent(QEvent* event) override;
    QMenu* createPopupMenu() override;
    AccountList* m_accounts;
    Ui::AccountListPage* ui;
};
