#include "MSALoginDialog.h"
#include "Application.h"

#include "ui_MSALoginDialog.h"

#include "DesktopServices.h"
#include "minecraft/auth/AuthFlow.h"

#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QUrl>
#include <QtWidgets/QPushButton>

MSALoginDialog::MSALoginDialog(QWidget* parent) : QDialog(parent), ui(new Ui::MSALoginDialog)
{
    ui->setupUi(this);

    // Browser OAuth only — hide device-code / microsoft.com/link UI
    ui->stackedWidget->setCurrentIndex(0);
    if (auto* page = ui->stackedWidget->widget(1)) {
        ui->stackedWidget->removeWidget(page);
        page->deleteLater();
    }

    ui->buttonBox->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    ui->loadingLabel->setText(tr("Sign in with Microsoft"));
    ui->status->setText(tr("Opening your browser..."));
}

int MSALoginDialog::exec()
{
    if (APPLICATION->getMSAClientID().isEmpty()) {
        QMessageBox::warning(this, tr("Microsoft login not configured"),
                             tr("No Azure client ID was found. Create auth-settings.ini next to the launcher executable."));
        return QDialog::Rejected;
    }

    m_account = MinecraftAccount::createBlankMSA();
    // Browser authorization-code flow (NOT device code / microsoft.com/link)
    m_login_task = m_account->login(false);
    connect(m_login_task.get(), &Task::failed, this, &MSALoginDialog::onTaskFailed);
    connect(m_login_task.get(), &Task::succeeded, this, &QDialog::accept);
    connect(m_login_task.get(), &Task::aborted, this, &MSALoginDialog::reject);
    connect(m_login_task.get(), &Task::status, this, &MSALoginDialog::onFlowStatus);
    connect(m_login_task.get(), &AuthFlow::authorizeWithBrowser, this, &MSALoginDialog::authorizeWithBrowser);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, m_login_task.get(), &Task::abort);
    QMetaObject::invokeMethod(m_login_task.get(), &Task::start, Qt::QueuedConnection);

    return QDialog::exec();
}

MSALoginDialog::~MSALoginDialog()
{
    delete ui;
}

void MSALoginDialog::onTaskFailed(QString reason)
{
    m_login_task->disconnect();
    ui->stackedWidget->setCurrentIndex(0);
    auto lines = reason.split('\n');
    QString processed;
    for (auto line : lines) {
        if (line.size()) {
            processed += "<font color='red'>" + line.toHtmlEscaped() + "</font><br />";
        } else {
            processed += "<br />";
        }
    }
    ui->status->setText(processed);
    if (m_login_task) {
        ui->loadingLabel->setText(m_login_task->getStatus());
    }
    disconnect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, m_login_task.get(), &Task::abort);
    connect(ui->buttonBox->button(QDialogButtonBox::Cancel), &QPushButton::clicked, this, &MSALoginDialog::reject);
}

void MSALoginDialog::authorizeWithBrowser(const QUrl& url)
{
    ui->stackedWidget->setCurrentIndex(0);
    ui->loadingLabel->setText(tr("Complete sign-in in your browser"));
    ui->status->setText(tr("A browser window was opened. Finish Microsoft login there, then return here.\n\n"
                           "If nothing opened, visit:\n%1")
                            .arg(url.toString()));
    if (!DesktopServices::openUrl(url)) {
        QApplication::clipboard()->setText(url.toString());
        ui->status->setText(tr("Could not open the browser. The login URL was copied to the clipboard:\n%1").arg(url.toString()));
    }
}

void MSALoginDialog::onFlowStatus(QString status)
{
    ui->stackedWidget->setCurrentIndex(0);
    ui->status->setText(status);
}

MinecraftAccountPtr MSALoginDialog::newAccount(QWidget* parent)
{
    MSALoginDialog dlg(parent);
    if (dlg.exec() == QDialog::Accepted) {
        return dlg.m_account;
    }
    return nullptr;
}
