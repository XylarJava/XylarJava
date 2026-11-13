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

void MSAWamStep::useFallback(const QString& reason)
{
    if (!reason.isEmpty())
        qWarning() << "Windows token broker:" << reason << (m_silent ? "- using the saved refresh token" : "- using a device code");
    if (m_silent) {
        m_fallback = makeShared<MSARefreshStep>(m_data);
    } else {
        auto deviceCode = makeShared<MSADeviceCodeStep>(m_data);
        connect(deviceCode.get(), &MSADeviceCodeStep::authorizeWithBrowser, this, &MSAWamStep::authorizeWithDeviceCode);
        // a fresh sign-in starts from a clean account, like the broker path does
        *m_data = AccountData();
        m_data->msaClientID = m_clientId;
        m_fallback = deviceCode;
    }
    connect(m_fallback.get(), &AuthStep::finished, this, &AuthStep::finished);
    m_fallback->perform();
}

void MSAWamStep::abort()
{
    if (m_fallback)
        m_fallback->abort();
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
            useFallback(QString());
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
                    emit self->finished(AccountTaskState::STATE_FAILED_SOFT, tr("Microsoft sign-in was cancelled."));
                    return;
                }
                if (result.status != wambroker::Status::Success) {
                    self->useFallback(QString::fromStdString(result.error));
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
                // Keep a refresh token from a previous browser login as a broker fallback.
                token.extra.insert(kWamAccountId, QString::fromStdString(result.accountId));
                qDebug() << "Windows token broker signed in" << QString::fromStdString(result.accountName);
                emit self->finished(AccountTaskState::STATE_WORKING, tr("Got MSA token"));
            },
            Qt::QueuedConnection);
    }).detach();
#else
    useFallback(QString());
#endif
}
