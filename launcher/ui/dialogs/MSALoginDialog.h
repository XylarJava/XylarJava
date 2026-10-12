#pragma once

#include <QtWidgets/QDialog>

#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/MinecraftAccount.h"

namespace Ui {
class MSALoginDialog;
}

class MSALoginDialog : public QDialog {
    Q_OBJECT

   public:
    ~MSALoginDialog();

    static MinecraftAccountPtr newAccount(QWidget* parent);
    int exec() override;

   private:
    explicit MSALoginDialog(QWidget* parent = 0);

   protected slots:
    void onTaskFailed(QString reason);
    void onFlowStatus(QString status);
    void authorizeWithBrowser(const QUrl& url);

   private:
    Ui::MSALoginDialog* ui;
    MinecraftAccountPtr m_account;
    shared_qobject_ptr<AuthFlow> m_login_task;
};
