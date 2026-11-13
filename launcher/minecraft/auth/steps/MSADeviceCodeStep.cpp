#include "MSADeviceCodeStep.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#ifdef Q_OS_LINUX
#include <dlfcn.h>
#include <execinfo.h>
#endif

#include "Application.h"
#include "minecraft/auth/steps/MSAScopes.h"

// https://learn.microsoft.com/en-us/entra/identity-platform/v2-oauth2-device-code
// Same requests and loop as XylarJava for Xbox (RequestDeviceCode, PollDeviceToken, AddMicrosoftAccount).

namespace {
const char* DEVICE_CODE_URL = "https://login.microsoftonline.com/consumers/oauth2/v2.0/devicecode";
const char* TOKEN_URL = "https://login.microsoftonline.com/consumers/oauth2/v2.0/token";

QJsonObject parseObject(const QByteArray& body)
{
    return QJsonDocument::fromJson(body).object();
}

/// Who called abort(): written to the log so an unexpected cancel can be traced.
QString callers()
{
#ifdef Q_OS_LINUX
    void* frames[12];
    const int count = backtrace(frames, 12);
    QStringList out;
    for (int i = 1; i < count; i++) {
        Dl_info info;
        if (dladdr(frames[i], &info) && info.dli_fbase)
            out << QStringLiteral("%1+0x%2")
                       .arg(QString::fromLocal8Bit(info.dli_fname).section('/', -1))
                       .arg(quintptr(frames[i]) - quintptr(info.dli_fbase), 0, 16);
    }
    return out.join(QStringLiteral(" <- "));
#else
    return QString();
#endif
}
}  // namespace

MSADeviceCodeStep::MSADeviceCodeStep(AccountData* data) : AuthStep(data)
{
    m_clientId = APPLICATION->getMSAClientID();
    m_pollTimer.setSingleShot(true);
    connect(&m_pollTimer, &QTimer::timeout, this, &MSADeviceCodeStep::poll);
}

QString MSADeviceCodeStep::describe()
{
    return tr("Signing in with Microsoft.");
}

void MSADeviceCodeStep::post(const QString& url, const QByteArray& form, std::function<void(int status, const QByteArray& body)> done)
{
    QNetworkRequest request{ QUrl(url) };
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("Accept", "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, APPLICATION->getUserAgent());
    request.setTransferTimeout(30000);
    m_reply = APPLICATION->network()->post(request, form);
    QNetworkReply* reply = m_reply;
    // Microsoft answers "not approved yet" with HTTP 400 and a JSON body: read status and body
    // whatever the status is, like the Xbox version's HttpPostString.
    connect(reply, &QNetworkReply::finished, this, [this, reply, done] {
        reply->deleteLater();
        if (m_isAborted || m_finished)
            return;
        done(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), reply->readAll());
    });
}

void MSADeviceCodeStep::perform()
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    form.addQueryItem(QStringLiteral("scope"), QString::fromLatin1(kMSAScopes));
    post(QString::fromLatin1(DEVICE_CODE_URL), form.query(QUrl::FullyEncoded).toUtf8(), [this](int status, const QByteArray& body) {
        const QJsonObject json = parseObject(body);
        if (status < 200 || status >= 300) {
            const QString error = json.value("error_description").toString(json.value("error").toString());
            qWarning() << "Device code request failed: HTTP" << status << error;
            return fail(tr("Microsoft didn't give a sign-in code (%1). Check your connection and try again.")
                            .arg(error.isEmpty() ? tr("HTTP %1").arg(status) : error));
        }
        const QString userCode = json.value("user_code").toString();
        m_deviceCode = json.value("device_code").toString();
        const QString verificationUri = json.value("verification_uri").toString();
        const int expiresIn = json.value("expires_in").toInt(900);
        m_interval = qMax(1, json.value("interval").toInt(5));
        if (userCode.isEmpty() || m_deviceCode.isEmpty() || verificationUri.isEmpty())
            return fail(tr("Microsoft's sign-in code was incomplete. Try again."));
        qDebug() << "Device auth code received, expires in" << expiresIn << "s, interval" << m_interval << "s";
        m_expiresAt = QDeadlineTimer(qint64(expiresIn) * 1000);
        emit authorizeWithBrowser(verificationUri, userCode, expiresIn);
        schedulePoll();
    });
}

void MSADeviceCodeStep::schedulePoll()
{
    if (m_isAborted || m_finished)
        return;
    if (m_expiresAt.hasExpired())
        return fail(tr("The code expired. Add the account again to get a new code."));
    m_pollTimer.start(int(qMin<qint64>(qint64(m_interval) * 1000, qMax<qint64>(1000, m_expiresAt.remainingTime()))));
}

void MSADeviceCodeStep::poll()
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("urn:ietf:params:oauth:grant-type:device_code"));
    form.addQueryItem(QStringLiteral("client_id"), m_clientId);
    form.addQueryItem(QStringLiteral("device_code"), m_deviceCode);
    post(QString::fromLatin1(TOKEN_URL), form.query(QUrl::FullyEncoded).toUtf8(), [this](int status, const QByteArray& body) {
        const QJsonObject json = parseObject(body);
        if (status >= 200 && status < 300) {
            const QString accessToken = json.value("access_token").toString();
            if (accessToken.isEmpty())
                return fail(tr("Microsoft's answer had no access token. Try again."));
            m_finished = true;
            m_pollTimer.stop();
            m_data->msaClientID = m_clientId;
            m_data->msaToken.issueInstant = QDateTime::currentDateTimeUtc();
            m_data->msaToken.notAfter = QDateTime::currentDateTime().addSecs(json.value("expires_in").toInt());
            m_data->msaToken.extra = json.toVariantMap();
            m_data->msaToken.extra.remove("access_token");
            m_data->msaToken.extra.remove("refresh_token");
            m_data->msaToken.refresh_token = json.value("refresh_token").toString();
            m_data->msaToken.token = accessToken;
            emit finished(AccountTaskState::STATE_WORKING, tr("Got MSA token"));
            return;
        }
        const QString error = json.value("error").toString();
        if (error == QLatin1String("authorization_pending"))
            return schedulePoll();
        if (error == QLatin1String("slow_down")) {
            m_interval += 5;
            return schedulePoll();
        }
        if (error.isEmpty() && status == 0) {
            // No answer at all (connection dropped or timed out): keep the same code and ask again a bit later.
            qWarning() << "Device token poll got no answer, trying again";
            m_interval *= 2;
            return schedulePoll();
        }
        qWarning() << "Device token poll failed: HTTP" << status << error;
        if (error == QLatin1String("authorization_declined"))
            return fail(tr("Microsoft login was declined. Approve the request on the Microsoft page and try again."));
        if (error == QLatin1String("expired_token"))
            return fail(tr("The code expired. Add the account again to get a new code."));
        const QString description = json.value("error_description").toString();
        fail(error.isEmpty() ? tr("Microsoft token polling failed: HTTP %1").arg(status)
                             : QStringLiteral("%1: %2").arg(error, description));
    });
}

void MSADeviceCodeStep::fail(const QString& message)
{
    if (m_finished)
        return;
    m_finished = true;
    m_pollTimer.stop();
    emit finished(AccountTaskState::STATE_FAILED_HARD, message);
}

void MSADeviceCodeStep::abort()
{
    if (m_isAborted || m_finished)
        return;
    qWarning() << "Microsoft device sign-in cancelled; called from" << callers();
    m_isAborted = true;
    m_pollTimer.stop();
    if (m_reply)
        m_reply->abort();
    m_finished = true;
    emit finished(AccountTaskState::STATE_FAILED_HARD, tr("Microsoft sign-in was cancelled."));
}
