#include "MSARefreshStep.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include "Application.h"
#include "minecraft/auth/steps/MSAScopes.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"

MSARefreshStep::MSARefreshStep(AccountData* data) : AuthStep(data)
{
    m_clientId = APPLICATION->getMSAClientID();
}

QString MSARefreshStep::describe()
{
    return tr("Refreshing the Microsoft sign-in.");
}

void MSARefreshStep::perform()
{
    if (m_data->msaClientID != m_clientId) {
        emit finished(AccountTaskState::STATE_DISABLED, tr("This account was added with another app ID. Sign in with Microsoft again."));
        return;
    }
    if (m_data->msaToken.refresh_token.isEmpty()) {
        emit finished(AccountTaskState::STATE_DISABLED, tr("The saved Microsoft session is gone. Sign in with Microsoft again."));
        return;
    }

    QUrlQuery data;
    data.addQueryItem("grant_type", "refresh_token");
    data.addQueryItem("client_id", m_clientId);
    data.addQueryItem("refresh_token", m_data->msaToken.refresh_token);
    data.addQueryItem("scope", kMSAScopes);
    const QByteArray payload = data.query(QUrl::FullyEncoded).toUtf8();

    QUrl url("https://login.microsoftonline.com/consumers/oauth2/v2.0/token");
    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/x-www-form-urlencoded" },
        { "Accept", "application/json" },
    };
    auto [request, response] = Net::Upload::makeByteArray(url, payload);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(false);

    m_task.reset(new NetJob("MSARefreshStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);
    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });
    m_task->start();
}

void MSARefreshStep::abort()
{
    if (m_task)
        m_task->abort();
}

void MSARefreshStep::onRequestDone(QByteArray* response)
{
    const QJsonObject json = QJsonDocument::fromJson(*response).object();
    const QString error = json.value("error").toString();

    if (m_request->error() != QNetworkReply::NoError || !error.isEmpty()) {
        const QString description = json.value("error_description").toString().section(QLatin1Char('\n'), 0, 0).trimmed();
        qWarning() << "Microsoft token refresh failed:" << m_request->replyStatusCode() << error << description;
        if (error == "invalid_grant" || error == "interaction_required" || error == "consent_required") {
            // The refresh token was revoked or expired: only a new sign-in helps.
            emit finished(AccountTaskState::STATE_FAILED_HARD, tr("The Microsoft session expired. Sign in with Microsoft again."));
        } else if (!error.isEmpty() || Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT,
                          tr("Couldn't refresh the Microsoft sign-in: %1").arg(description.isEmpty() ? error : description));
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE, tr("Couldn't reach Microsoft to refresh the sign-in."));
        }
        return;
    }

    const QString accessToken = json.value("access_token").toString();
    if (accessToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Microsoft's answer had no access token."));
        return;
    }

    const QDateTime now = QDateTime::currentDateTimeUtc();
    const int expiresIn = json.value("expires_in").toInt(3600);
    m_data->msaToken.issueInstant = now;
    m_data->msaToken.notAfter = now.addSecs(expiresIn > 0 ? expiresIn : 3600);
    m_data->msaToken.token = accessToken;
    const QString refreshToken = json.value("refresh_token").toString();
    if (!refreshToken.isEmpty())
        m_data->msaToken.refresh_token = refreshToken;
    m_data->msaToken.extra = json.toVariantMap();
    emit finished(AccountTaskState::STATE_WORKING, tr("Got MSA token"));
}
