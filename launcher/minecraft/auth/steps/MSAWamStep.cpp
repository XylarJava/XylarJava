#include "MSAWamStep.h"

#include <QApplication>
#include <QDebug>
#include <QPointer>
#include <QWidget>

#include <thread>

#include "Application.h"

#ifdef XYLARJAVA_WAM
#include "minecraft/auth/WamBroker.h"
#endif

namespace {
// AccountData::msaToken.extra key holding the Windows WebAccount id of a broker account
const QString kWamAccountId = QStringLiteral("wam_account_id");
}  // namespace

MSAWamStep::MSAWamStep(AccountData* data, bool silent) : AuthStep(data), m_silent(silent)
{
    m_clientId = APPLICATION->getMSAClientID();
}

QString MSAWamStep::describe()
{
    return tr("Logging in with Microsoft account.");
}

void MSAWamStep::useBrowserFlow(const QString& reason)
{
    if (!reason.isEmpty())
        qWarning() << "Windows token broker:" << reason << "- using the browser sign-in instead";
    m_fallback.reset(new MSAStep(m_data, m_silent));
    connect(m_fallback.get(), &MSAStep::authorizeWithBrowser, this, &MSAWamStep::authorizeWithBrowser);
    connect(m_fallback.get(), &AuthStep::finished, this, &AuthStep::finished);
    m_fallback->perform();
}

void MSAWamStep::perform()
{
#ifdef XYLARJAVA_WAM
    const std::string clientId = m_clientId.toStdString();
    std::string accountId;
    void* window = nullptr;
    if (m_silent) {
        accountId = m_data->msaToken.extra.value(kWamAccountId).toString().toStdString();
        // accounts added in the browser keep using their refresh token
        if (accountId.empty() || m_data->msaClientID != m_clientId) {
            useBrowserFlow(QString());
            return;
        }
    }
    {
        // a refresh may run with no dialog open, so fall back to any launcher window
        QWidget* parent = QApplication::activeWindow();
        if (!parent && !QApplication::topLevelWidgets().isEmpty())
            parent = QApplication::topLevelWidgets().first();
        if (parent)
            window = reinterpret_cast<void*>(parent->winId());
    }

    QPointer<MSAWamStep> self(this);
    const bool silent = m_silent;
    std::thread([self, silent, clientId, accountId, window]() {
        wambroker::Result result =
            silent ? wambroker::getTokenSilently(clientId, accountId) : wambroker::requestToken(clientId, window);
        // the Microsoft account provider often refuses silent requests; its regular request returns
        // at once without UI while the account's consent holds. Only accept the same account back.
        if (silent && result.status == wambroker::Status::UserInteractionRequired && window) {
            wambroker::Result again = wambroker::requestToken(clientId, window);
            if (again.status == wambroker::Status::Success && again.accountId != accountId) {
                again.status = wambroker::Status::Failed;
                again.error = "Windows returned a different Microsoft account.";
            }
            result = again;
        }
        QMetaObject::invokeMethod(
            qApp,
            [self, result]() {
                if (!self)
                    return;
                if (result.status == wambroker::Status::Cancelled) {
                    emit self->finished(AccountTaskState::STATE_FAILED_HARD, tr("Microsoft sign-in was cancelled."));
                    return;
                }
                if (result.status != wambroker::Status::Success) {
                    self->useBrowserFlow(QString::fromStdString(result.error));
                    return;
                }
                if (!self->m_silent) {
                    *self->m_data = AccountData();
                    self->m_data->msaClientID = self->m_clientId;
                }
                const QDateTime now = QDateTime::currentDateTimeUtc();
                auto& token = self->m_data->msaToken;
                token.issueInstant = now;
                token.notAfter = result.expiresAt > 0 ? QDateTime::fromSecsSinceEpoch(result.expiresAt).toUTC() : now.addSecs(3600);
                token.token = QString::fromStdString(result.accessToken);
                // the broker keeps the refresh token itself
                token.refresh_token.clear();
                token.extra.insert(kWamAccountId, QString::fromStdString(result.accountId));
                qDebug() << "Windows token broker signed in" << QString::fromStdString(result.accountName);
                emit self->finished(AccountTaskState::STATE_WORKING, tr("Got MSA token"));
            },
            Qt::QueuedConnection);
    }).detach();
#else
    useBrowserFlow(QString());
#endif
}
