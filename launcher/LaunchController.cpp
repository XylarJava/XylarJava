#include "LaunchController.h"
#include "Application.h"
#include "launch/steps/PrintServers.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"

#include "ui/InstanceWindow.h"
#include "ui/dialogs/CustomMessageBox.h"
#include "ui/dialogs/MSALoginDialog.h"
#include "ui/dialogs/ProfileSelectDialog.h"
#include "ui/dialogs/ProfileSetupDialog.h"
#include "ui/dialogs/ProgressDialog.h"

#include <QInputDialog>
#include <QList>
#include <QPushButton>
#include <utility>

#include "BuildConfig.h"
#include "JavaCommon.h"
#include "launch/steps/TextPrint.h"
#include "minecraft/skins/XylarSkinClient.h"
#include "tasks/Task.h"
#include "ui/dialogs/AccountIdentity.h"
#include "ui/dialogs/ChooseOfflineNameDialog.h"
#include "ui/dialogs/ElyLoginDialog.h"

LaunchController::LaunchController() = default;

void LaunchController::executeTask()
{
    m_startedAt.start();
    if (!m_instance) {
        emitFailed(tr("No instance specified!"));
        return;
    }

    if (!JavaCommon::checkJVMArgs(m_instance->settings()->get("JvmArgs").toString(), m_parentWidget)) {
        emitFailed(tr("Invalid Java arguments specified. Please fix this first."));
        return;
    }

    login();
}

void LaunchController::decideAccount()
{
    if (m_accountToUse) {
        return;
    }

    // Select the account to use. If the instance has a specific account set, that will be used. Otherwise, the default account will be used
    auto* accounts = APPLICATION->accounts();
    const auto instanceAccountId = m_instance->settings()->get("InstanceAccountId").toString();
    const auto instanceAccountIndex = accounts->findAccountByProfileId(instanceAccountId);
    if (instanceAccountIndex == -1 || instanceAccountId.isEmpty()) {
        m_accountToUse = accounts->defaultAccount();
    } else {
        m_accountToUse = accounts->at(instanceAccountIndex);
    }

    if (!m_accountToUse) {
        // Check if there are any accounts at all
        if (accounts->count() == 0) {
            // No accounts exist, offer to create an offline account directly
            ChooseOfflineNameDialog dialog(tr("No account found. Please enter a username to create an offline account."), m_parentWidget);
            dialog.setWindowTitle(tr("Create Offline Account"));
            if (dialog.exec() == QDialog::Accepted) {
                const MinecraftAccountPtr account = MinecraftAccount::createOffline(dialog.getUsername());
                if (account && AccountIdentity::verify(m_parentWidget, account)) {
                    account->login()->start();  // The task will complete here.
                    accounts->addAccount(account);
                    m_accountToUse = account;
                    accounts->setDefaultAccount(account);
                }
            }
        } else {
            // Accounts exist, show profile selection dialog
            ProfileSelectDialog selectDialog(tr("Which account would you like to use?"), ProfileSelectDialog::GlobalDefaultCheckbox,
                                             m_parentWidget);

            selectDialog.exec();

            // Launch the instance with the selected account.
            m_accountToUse = selectDialog.selectedAccount();

            // If the user said to use the account as default, do that.
            if (selectDialog.useAsGlobalDefault() && m_accountToUse) {
                accounts->setDefaultAccount(m_accountToUse);
            }
        }
    }
}

LaunchDecision LaunchController::decideLaunchMode()
{
    m_actualLaunchMode = LaunchMode::Normal;
    // Offline accounts need nothing from Microsoft.
    if (!m_accountToUse || m_accountToUse->accountType() == AccountType::Offline)
        return LaunchDecision::Continue;

    switch (m_accountToUse->accountState()) {
        case AccountState::Online:
            return LaunchDecision::Continue;
        case AccountState::Unchecked:
        case AccountState::Working:
        case AccountState::Errored: {
            // The sign-in isn't checked yet (or is being checked): wait for it once, so the game gets a
            // session Mojang accepts. Without one the game starts but no online server lets the player in.
            if (m_triedRefresh) {
                // Still not checked (Microsoft or Mojang having trouble): play anyway, like when offline.
                return LaunchDecision::Continue;
            }
            m_triedRefresh = true;
            auto task = m_accountToUse->refresh();
            if (task && !task->isFinished()) {
                ProgressDialog dialog(m_parentWidget);
                dialog.setSkipButton(true, tr("Play anyway"));
                dialog.execWithTask(task.get());
            }
            return LaunchDecision::Undecided;
        }
        case AccountState::Offline:
            // No internet: single player still works.
            return LaunchDecision::Continue;
        case AccountState::Disabled:
        case AccountState::Expired:
        case AccountState::Gone: {
            const QString reason = m_accountToUse->lastError().isEmpty()
                                       ? tr("The sign-in of %1 expired").arg(m_accountToUse->profileName())
                                       : m_accountToUse->lastError();
            if (reauthenticateAccount(m_accountToUse, reason.endsWith('.') ? reason.chopped(1) : reason))
                return LaunchDecision::Undecided;
            return LaunchDecision::Abort;
        }
    }
    return LaunchDecision::Continue;
}

bool LaunchController::askPlayDemo() const
{
    QMessageBox box(m_parentWidget);
    box.setWindowTitle(tr("Play demo?"));
    QString text = m_accountToUse
                       ? tr("This account does not own Minecraft.\nYou need to purchase the game first to play the full version.")
                       : tr("No account was selected for launch.");
    text += tr("\n\nDo you want to play the demo?");
    box.setText(text);
    box.setIcon(QMessageBox::Warning);
    const auto* demoButton = box.addButton(tr("Play Demo"), QMessageBox::ButtonRole::YesRole);
    auto* cancelButton = box.addButton(tr("Cancel"), QMessageBox::ButtonRole::NoRole);
    box.setDefaultButton(cancelButton);

    box.exec();
    return box.clickedButton() == demoButton;
}

QString LaunchController::askOfflineName(const QString& playerName, bool* ok) const
{
    if (ok != nullptr) {
        *ok = false;
    }

    QString message;
    switch (m_actualLaunchMode) {
        case LaunchMode::Normal:
            Q_ASSERT(false);
            return "";
        case LaunchMode::Demo:
            message = tr("Choose your demo mode player name");
            break;
        case LaunchMode::Offline:
            if (m_wantedLaunchMode == LaunchMode::Normal) {
                message = tr("You are not connected to the Internet, launching in offline mode\n\n");
            }
            message += tr("Choose your offline mode player name");
            break;
    }

    const QString lastOfflinePlayerName = APPLICATION->settings()->get("LastOfflinePlayerName").toString();
    QString usedname = lastOfflinePlayerName.isEmpty() ? playerName : lastOfflinePlayerName;

    ChooseOfflineNameDialog dialog(message, m_parentWidget);
    dialog.setWindowTitle(tr("Player name"));
    dialog.setUsername(usedname);
    if (dialog.exec() != QDialog::Accepted) {
        return {};
    }

    usedname = dialog.getUsername();
    APPLICATION->settings()->set("LastOfflinePlayerName", usedname);

    if (ok != nullptr) {
        *ok = true;
    }
    return usedname;
}

void LaunchController::login()
{
    decideAccount();

    // Check if an account was selected after decideAccount()
    if (!m_accountToUse) {
        emitAborted();
        return;
    }

    LaunchDecision decision = decideLaunchMode();
    while (decision == LaunchDecision::Undecided) {
        decision = decideLaunchMode();
    }
    if (decision == LaunchDecision::Abort) {
        emitAborted();
        return;
    }

    if (m_actualLaunchMode == LaunchMode::Demo) {
        if (m_wantedLaunchMode == LaunchMode::Demo || askPlayDemo()) {
            bool ok = false;
            auto name = askOfflineName("Player", &ok);
            if (ok) {
                m_session = std::make_shared<AuthSession>();
                m_session->MakeDemo(name, MinecraftAccount::uuidFromUsername(name).toString(QUuid::Id128));
                if (!AccountIdentity::verify(m_parentWidget, MinecraftAccount::createOffline(name))) {
                    emitFailed(tr("Username ownership could not be verified."));
                    return;
                }
                launchInstance();
                return;
            }
        }

        emitFailed(tr("No account selected for launch"));
        return;
    }

    m_session = std::make_shared<AuthSession>();
    m_session->launchMode = m_actualLaunchMode;
    m_accountToUse->fillSession(m_session);

    if (m_accountToUse->accountType() != AccountType::Offline) {
        if (m_actualLaunchMode == LaunchMode::Normal && !m_accountToUse->hasProfile()) {
            // Now handle setting up a profile name here...
            if (ProfileSetupDialog dialog(m_accountToUse, m_parentWidget); dialog.exec() != QDialog::Accepted) {
                emitAborted();
                return;
            }
        }

        if (m_actualLaunchMode == LaunchMode::Offline && m_accountToUse->accountType() != AccountType::Offline) {
            bool ok = false;
            QString name = m_offlineName;
            if (name.isEmpty()) {
                name = askOfflineName(m_session->player_name, &ok);
                if (!ok) {
                    emitAborted();
                    return;
                }
            }
            m_session->MakeOffline(name);
        }
    }

    if (m_session->launchMode != LaunchMode::Demo) {
        auto identity = m_accountToUse;
        if (m_session->user_type == QStringLiteral("offline") && identity->accountType() != AccountType::Offline)
            identity = MinecraftAccount::createOffline(m_session->player_name);
        if (!AccountIdentity::verify(m_parentWidget, identity)) {
            emitFailed(tr("Username ownership could not be verified."));
            return;
        }
        if (m_session->user_type == QStringLiteral("offline") && APPLICATION->skinClient()) {
            const auto key = APPLICATION->skinClient()->playerKey(XylarSkinClient::computeOfflineUUID(m_session->player_name));
            m_session->access_token = QStringLiteral("xylar:") + key;
            m_session->session = QStringLiteral("token:") + m_session->access_token + QLatin1Char(':') + m_session->uuid;
        }
    }
    launchInstance();
}

bool LaunchController::reauthenticateAccount(const MinecraftAccountPtr& account, const QString& reason)
{
    auto button = QMessageBox::warning(
        m_parentWidget, tr("Account refresh failed"), tr("%1. Do you want to reauthenticate this account?").arg(reason),
        QMessageBox::StandardButton::Yes | QMessageBox::StandardButton::No, QMessageBox::StandardButton::Yes);
    if (button == QMessageBox::StandardButton::Yes) {
        auto* accounts = APPLICATION->accounts();
        const bool isDefault = accounts->defaultAccount() == account;
        if (account->accountType() != AccountType::Offline) {
            auto newAccount = account->accountType() == AccountType::Ely ? ElyAccountDialog::login(m_parentWidget)
                                                                         : MSALoginDialog::newAccount(m_parentWidget);

            if (newAccount != nullptr) {
                // The old entry goes only once the new sign-in worked: cancelling keeps the account.
                accounts->removeAccount(accounts->index(accounts->findAccountByProfileId(account->profileId())));
                accounts->addAccount(newAccount);

                if (isDefault) {
                    accounts->setDefaultAccount(newAccount);
                }

                if (m_accountToUse == account) {
                    m_accountToUse = nullptr;
                    decideAccount();
                }
                return true;
            }
        }
    }

    return false;
}

void LaunchController::launchInstance()
{
    Q_ASSERT(m_instance != nullptr);
    Q_ASSERT(m_session.get() != nullptr);

    if (!m_instance->reloadSettings()) {
        QMessageBox::critical(m_parentWidget, tr("Error!"), tr("Couldn't load the instance profile."));
        emitFailed(tr("Couldn't load the instance profile."));
        return;
    }

    m_launcher = m_instance->createLaunchTask(m_session, m_targetToJoin);
    if (!m_launcher) {
        emitFailed(tr("Couldn't instantiate a launcher."));
        return;
    }
    m_reachedLaunch = true;

    const auto* console = qobject_cast<InstanceWindow*>(m_parentWidget);
    const auto showConsole = m_instance->settings()->get("ShowConsole").toBool();
    if (!console && showConsole) {
        APPLICATION->showInstanceWindow(m_instance, "console");
    }
    connect(m_launcher, &LaunchTask::readyForLaunch, this, &LaunchController::readyForLaunch);
    connect(m_launcher, &LaunchTask::succeeded, this, &LaunchController::onSucceeded);
    connect(m_launcher, &LaunchTask::failed, this, &LaunchController::onFailed);
    connect(m_launcher, &LaunchTask::requestProgress, this, &LaunchController::onProgressRequested);

    // Prepend Online and Auth Status
    QString online_mode;
    if (m_actualLaunchMode == LaunchMode::Normal) {
        online_mode = "online";

        // Prepend Server Status
        const QStringList servers = { "login.microsoftonline.com", "session.minecraft.net", "textures.minecraft.net", "api.mojang.com" };

        m_launcher->prependStep(makeShared<PrintServers>(m_launcher, servers));
    } else {
        online_mode = m_actualLaunchMode == LaunchMode::Demo ? "demo" : "offline";
    }

    m_launcher->prependStep(makeShared<TextPrint>(m_launcher, "Launched instance in " + online_mode + " mode\n", MessageLevel::Launcher));

    // Prepend Version
    {
        auto versionString = QString("%1 version: %2 (%3)")
                                 .arg(BuildConfig.LAUNCHER_DISPLAYNAME, BuildConfig.printableVersionString(), BuildConfig.BUILD_PLATFORM);
        m_launcher->prependStep(makeShared<TextPrint>(m_launcher, versionString + "\n", MessageLevel::Launcher));
    }
    m_launcher->start();
}

void LaunchController::readyForLaunch()
{
    if (!m_profiler) {
        m_launcher->proceed();
        return;
    }

    QString error;
    if (!m_profiler->check(&error)) {
        m_launcher->abort();
        emitFailed("Profiler startup failed!");
        QMessageBox::critical(m_parentWidget, tr("Error!"), tr("Profiler check for %1 failed: %2").arg(m_profiler->name(), error));
        return;
    }
    BaseProfiler* profilerInstance = m_profiler->createProfiler(m_launcher->instance(), this);

    connect(profilerInstance, &BaseProfiler::readyToLaunch, [this](const QString& message) {
        QMessageBox msg(m_parentWidget);
        msg.setText(tr("The game launch is delayed until you press the "
                       "button. This is the right time to setup the profiler, as the "
                       "profiler server is running now.\n\n%1")
                        .arg(message));
        msg.setWindowTitle(tr("Waiting."));
        msg.setIcon(QMessageBox::Information);
        msg.addButton(tr("&Launch"), QMessageBox::AcceptRole);
        msg.exec();
        m_launcher->proceed();
    });
    connect(profilerInstance, &BaseProfiler::abortLaunch, [this](const QString& message) {
        QMessageBox msg;
        msg.setText(tr("Couldn't start the profiler: %1").arg(message));
        msg.setWindowTitle(tr("Error"));
        msg.setIcon(QMessageBox::Critical);
        msg.addButton(QMessageBox::Ok);
        msg.setModal(true);
        msg.exec();
        m_launcher->abort();
        emitFailed("Profiler startup failed!");
    });
    profilerInstance->beginProfiling(m_launcher);
}

bool LaunchController::failedWhileStarting() const
{
    // Two minutes covers downloads and the game's own loading screen.
    return m_reachedLaunch && getState() == State::Failed && m_startedAt.isValid() && m_startedAt.elapsed() < 120000;
}

bool LaunchController::failedAfterLaunch() const
{
    return m_reachedLaunch && getState() == State::Failed;
}

void LaunchController::onSucceeded()
{
    emitSucceeded();
}

void LaunchController::onFailed(QString reason)
{
    if (m_instance->settings()->get("ShowConsoleOnError").toBool()) {
        APPLICATION->showInstanceWindow(m_instance, "console");
    }
    emitFailed(std::move(reason));
}

void LaunchController::onProgressRequested(Task* task) const
{
    ProgressDialog progDialog(m_parentWidget);
    progDialog.setSkipButton(true, tr("Abort"));
    m_launcher->proceed();
    progDialog.execWithTask(task);
}

bool LaunchController::abort()
{
    if (!m_launcher) {
        return true;
    }
    if (!m_launcher->canAbort()) {
        return false;
    }
    auto response = CustomMessageBox::selectable(m_parentWidget, tr("Kill Minecraft?"),
                                                 tr("This can cause the instance to get corrupted and should only be used if Minecraft "
                                                    "is frozen for some reason"),
                                                 QMessageBox::Question, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes)
                        ->exec();
    if (response == QMessageBox::Yes) {
        return m_launcher->abort();
    }
    return false;
}
