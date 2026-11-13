#include "AccountListPage.h"
#include "ui/dialogs/skins/SkinManageDialog.h"
#include "ui/dialogs/skins/XylarSkinsPage.h"
#include "ui_AccountListPage.h"

#include <QItemSelectionModel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>

#include <QCoreApplication>
#include <QDebug>

#include "ui/dialogs/ChooseOfflineNameDialog.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/MSALoginDialog.h"

#include "Application.h"
#include "DesktopServices.h"
#include "ui/dialogs/AccountIdentity.h"
#include "ui/dialogs/ElyLoginDialog.h"

AccountListPage::AccountListPage(QWidget* parent) : QMainWindow(parent), ui(new Ui::AccountListPage)
{
    ui->setupUi(this);
    // The only page that keeps the classic button bar: adding, removing and picking accounts at a glance.
    ui->toolBar->showAgain();
    ui->listView->setEmptyString(
        tr("Welcome!\n"
           "Choose Add Microsoft, Add Ely.by or Add Offline to create a profile."));
    ui->listView->setEmptyMode(VersionListView::String);
    ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

    {
        auto freeMinecraft = new QAction(QIcon::fromTheme(QStringLiteral("minecraft")), QStringLiteral("Free Minecraft 100% not fake"), this);
        freeMinecraft->setToolTip(QStringLiteral("Free Minecraft 100% not fake"));
        connect(freeMinecraft, &QAction::triggered, this,
                [] { DesktopServices::openUrl(QUrl(QStringLiteral("https://www.youtube.com/watch?v=dQw4w9WgXcQ"))); });
        ui->toolBar->addSeparator();
        ui->toolBar->addAction(freeMinecraft);
    }

    m_accounts = APPLICATION->accounts();
    auto* ely = new QAction(tr("Add Ely.by"), this);
    ui->toolBar->insertActionAfter(ui->actionAddMicrosoft, ely);
    connect(ely, &QAction::triggered, this, [this] {
        auto account = ElyAccountDialog::login(this);
        if (!account || !AccountIdentity::verify(this, account))
            return;
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1)
            m_accounts->setDefaultAccount(account);
    });

    ui->listView->setModel(m_accounts);
    ui->listView->header()->setSectionResizeMode(AccountList::VListColumns::ProfileNameColumn, QHeaderView::Stretch);
    ui->listView->header()->setSectionResizeMode(AccountList::VListColumns::TypeColumn, QHeaderView::ResizeToContents);
    ui->listView->header()->setSectionResizeMode(AccountList::VListColumns::StatusColumn, QHeaderView::ResizeToContents);
    ui->listView->setSelectionMode(QAbstractItemView::SingleSelection);

    // Expand the account column

    QItemSelectionModel* selectionModel = ui->listView->selectionModel();

    connect(selectionModel, &QItemSelectionModel::selectionChanged,
            [this]([[maybe_unused]] const QItemSelection& sel, [[maybe_unused]] const QItemSelection& dsel) { updateButtonStates(); });
    connect(ui->listView, &VersionListView::customContextMenuRequested, this, &AccountListPage::ShowContextMenu);
    connect(ui->listView, &VersionListView::activated, this,
            [this](const QModelIndex& index) { m_accounts->setDefaultAccount(m_accounts->at(index.row())); });

    connect(m_accounts, &AccountList::listChanged, this, &AccountListPage::listChanged);
    connect(m_accounts, &AccountList::listActivityChanged, this, &AccountListPage::listChanged);
    connect(m_accounts, &AccountList::defaultAccountChanged, this, &AccountListPage::listChanged);

    updateButtonStates();
}

AccountListPage::~AccountListPage()
{
    delete ui;
}

void AccountListPage::retranslate()
{
    ui->retranslateUi(this);
}

void AccountListPage::ShowContextMenu(const QPoint& pos)
{
    auto menu = ui->toolBar->createContextMenu(this, tr("Context menu"));
    menu->exec(ui->listView->mapToGlobal(pos));
    delete menu;
}

void AccountListPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
    }
    QMainWindow::changeEvent(event);
}

QMenu* AccountListPage::createPopupMenu()
{
    QMenu* filteredMenu = QMainWindow::createPopupMenu();
    filteredMenu->removeAction(ui->toolBar->toggleViewAction());
    return filteredMenu;
}

void AccountListPage::listChanged()
{
    updateButtonStates();
}

void AccountListPage::on_actionAddMicrosoft_triggered()
{
    if (APPLICATION->getMSAClientID().isEmpty()) {
        QMessageBox::warning(
            this, tr("Microsoft login not configured"),
            tr("No Azure client ID was found.\n\n"
               "Create a file named <b>auth-settings.ini</b> next to %1 with:\n\n"
               "[Auth]\n"
               "MSAClientID=your-azure-app-client-id\n\n"
               "Use your own Azure App Registration (Mobile and desktop, public client flows enabled). "
               "See auth-settings.ini.example for a template.")
                .arg(QCoreApplication::applicationDirPath()));
        return;
    }

    auto account = MSALoginDialog::newAccount(this);
    if (account && AccountIdentity::verify(this, account, nullptr, true)) {
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
}

void AccountListPage::on_actionAddOffline_triggered()
{
    ChooseOfflineNameDialog dialog(tr("Please enter your desired username to add your offline account."), this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    if (const MinecraftAccountPtr account = MinecraftAccount::createOffline(dialog.getUsername())) {
        if (!AccountIdentity::verify(this, account))
            return;
        account->login()->start();
        m_accounts->addAccount(account);
        if (m_accounts->count() == 1) {
            m_accounts->setDefaultAccount(account);
        }
    }
}

void AccountListPage::on_actionRemove_triggered()
{
    auto response = CustomMessageBox::selectable(this, tr("Remove account?"), tr("Do you really want to delete this account?"),
                                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
                        ->exec();
    if (response != QMessageBox::Yes) {
        return;
    }
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        m_accounts->removeAccount(selected);
    }
}

void AccountListPage::on_actionRefresh_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        m_accounts->requestRefresh(account->internalId());
    }
}

void AccountListPage::on_actionSetDefault_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        m_accounts->setDefaultAccount(account);
    }
}

void AccountListPage::on_actionNoDefault_triggered()
{
    m_accounts->setDefaultAccount(nullptr);
}

void AccountListPage::updateButtonStates()
{
    // If there is no selection, disable buttons that require something selected.
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    bool hasSelection = !selection.empty();
    bool accountIsReady = false;
    bool accountIsOnline = false;
    bool accountCanMoveUp = false;
    bool accountCanMoveDown = false;
    if (hasSelection) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        accountIsReady = !account->isActive();
        accountIsOnline = account->accountType() != AccountType::Offline;

        accountCanMoveUp = selected.row() > 0;
        int indexOfLast = m_accounts->count() - 1;
        accountCanMoveDown = selected.row() < indexOfLast;
    }
    ui->actionRemove->setEnabled(accountIsReady);
    ui->actionSetDefault->setEnabled(accountIsReady);
    ui->actionManageSkins->setEnabled(accountIsReady && accountIsOnline);
    // XylarSkins works for both online and offline accounts
    ui->actionXylarSkins->setEnabled(accountIsReady);
    ui->actionRefresh->setEnabled(accountIsReady && accountIsOnline);

    if (m_accounts->defaultAccount().get() == nullptr) {
        ui->actionNoDefault->setEnabled(false);
        ui->actionNoDefault->setChecked(true);
    } else {
        ui->actionNoDefault->setEnabled(true);
        ui->actionNoDefault->setChecked(false);
    }
    ui->actionMoveUp->setEnabled(accountCanMoveUp);
    ui->actionMoveDown->setEnabled(accountCanMoveDown);
    ui->listView->resizeColumnToContents(3);
}

void AccountListPage::on_actionManageSkins_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        if (account->accountType() == AccountType::Ely) {
            ElyAccountDialog::showSkin(this, account);
            return;
        }
        SkinManageDialog dialog(this, account);
        dialog.exec();
    }
}

void AccountListPage::on_actionMoveUp_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        m_accounts->moveAccount(selected, -1);
    }
}

void AccountListPage::on_actionMoveDown_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        m_accounts->moveAccount(selected, 1);
    }
}

void AccountListPage::on_actionXylarSkins_triggered()
{
    QModelIndexList selection = ui->listView->selectionModel()->selectedIndexes();
    if (selection.size() > 0) {
        QModelIndex selected = selection.first();
        MinecraftAccountPtr account = selected.data(AccountList::PointerRole).value<MinecraftAccountPtr>();
        if (account->accountType() == AccountType::Ely) {
            ElyAccountDialog::showSkin(this, account);
            return;
        }
        XylarSkinsPage dialog(this, account);
        dialog.exec();
    }
}
