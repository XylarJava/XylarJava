#include "AccountIdentity.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include "Application.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/skins/XylarSkinClient.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {
class IdentityTask : public Task {
   public:
    explicit IdentityTask(MinecraftAccountPtr account, bool transfer) : m_account(account), m_client(this), m_transfer(transfer || account->accountData()->msaToken.extra.value("xylar_transfer_pending").toBool()) {}
    void executeTask() override
    {
        auto* global = APPLICATION->skinClient();
        m_client.setBackendUrl(APPLICATION->settings()->get("XylarSkinBackendUrl").toString());
        m_client.setAuthToken(APPLICATION->settings()->get("XylarSkinToken").toString());
        if (global)
            m_client.setCacheDir(global->cacheDir());
        setStatus(tr("Verifying ownership of %1…").arg(m_account->profileName()));
        connect(&m_client, &XylarSkinClient::playerRegistered, this, [this](const QString& uuid) {
            m_account->accountData()->yggdrasilToken.extra["xylarProfileId"] = uuid;
            m_account->accountData()->msaToken.extra.remove("xylar_transfer_pending");
            APPLICATION->accounts()->saveList();
            emitSucceeded();
        });
        connect(&m_client, &XylarSkinClient::requestFailed, this, [this](const QString&, const QString& error, int status) {
            // An already owned offline profile remains usable without Internet. Server rejections
            // (including an outdated backend) never become an offline bypass.
            if (status == 0 && m_account->accountType() == AccountType::Offline &&
                !m_client.playerKey(XylarSkinClient::computeOfflineUUID(m_account->profileName())).isEmpty())
                emitSucceeded();
            else
                emitFailed(error);
        });
        const auto type = m_account->accountType();
        m_client.registerPlayer(m_account->profileName(), type == AccountType::Offline ? QString() : m_account->profileId(),
                                type == AccountType::Offline ? QString() : m_account->accessToken(),
                                type == AccountType::MSA   ? QStringLiteral("microsoft")
                                : type == AccountType::Ely ? QStringLiteral("ely")
                                                           : QStringLiteral("offline"),
                                m_account->accountData()->yggdrasilToken.extra.value("xylarProfileId").toString(), m_transfer);
    }

   private:
    MinecraftAccountPtr m_account;
    XylarSkinClient m_client;
    bool m_transfer;
};
}  // namespace

bool AccountIdentity::verify(QWidget* parent, const MinecraftAccountPtr& account, QString* inlineError, bool transferSession)
{
    APPLICATION->initSkinClient();
    IdentityTask task(account, transferSession);
    ProgressDialog progress(parent);
    const auto result = progress.execWithTask(&task);
    if (result == QDialog::Accepted && task.wasSuccessful())
        return true;
    if (task.isFinished() && !task.wasSuccessful()) {
        if (inlineError) {
            *inlineError = task.failReason();
            return false;
        }
        QMessageBox message(QMessageBox::Warning, QObject::tr("Username could not be verified"), task.failReason(), QMessageBox::Cancel,
                            parent);
        auto* importButton = message.addButton(QObject::tr("Import my profile access…"), QMessageBox::ActionRole);
        message.exec();
        if (message.clickedButton() == importButton && APPLICATION->skinClient()) {
            const auto path = QFileDialog::getOpenFileName(parent, QObject::tr("Import profile access"), QString(),
                                                           QObject::tr("XylarJava profile access (*.xylar-skin-access)"));
            if (path.isEmpty())
                return false;
            QFile file(path);
            QString error;
            if (!file.open(QIODevice::ReadOnly) ||
                !APPLICATION->skinClient()->importPlayerAccess(XylarSkinClient::computeOfflineUUID(account->profileName()),
                                                               file.read(16384), &error)) {
                QMessageBox::warning(parent, QObject::tr("Cannot import access"), error.isEmpty() ? file.errorString() : error);
                return false;
            }
            return verify(parent, account);
        }
    }
    return false;
}
