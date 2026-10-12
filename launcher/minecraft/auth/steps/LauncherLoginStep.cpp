#include "LauncherLoginStep.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>

#include "Application.h"
#include "Logging.h"
#include "minecraft/auth/Parsers.h"
#include "net/NetUtils.h"
#include "net/RawHeaderProxy.h"
#include "net/Upload.h"

namespace {
QString buildIdentityToken(const QString& uhs, const QString& xToken)
{
    return QStringLiteral("XBL3.0 x=%1;%2").arg(uhs, xToken);
}

QString parseMinecraftAuthError(const QByteArray& response)
{
    // Retries can concatenate bodies; take the first JSON object.
    const int start = response.indexOf('{');
    const int end = response.indexOf('}', start);
    const QByteArray slice = (start >= 0 && end > start) ? response.mid(start, end - start + 1) : response;

    QJsonParseError jsonError;
    const QJsonDocument doc = QJsonDocument::fromJson(slice, &jsonError);
    if (jsonError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const auto obj = doc.object();
    const QString errorMessage = obj.value("errorMessage").toString();
    const QString errorCode = obj.value("errorCode").toString();
    if (!errorMessage.isEmpty()) {
        if (!errorCode.isEmpty()) {
            return QStringLiteral("%1 (%2)").arg(errorMessage, errorCode);
        }
        return errorMessage;
    }
    return obj.value("error").toString();
}
}  // namespace

LauncherLoginStep::LauncherLoginStep(AccountData* data) : AuthStep(data) {}

QString LauncherLoginStep::describe()
{
    return tr("Fetching Minecraft access token");
}

void LauncherLoginStep::perform()
{
    // Custom Azure apps must use login_with_xbox. /launcher/login is partner-only.
    startRequest(APPLICATION->useMSALauncherLoginEndpoint());
}

void LauncherLoginStep::startRequest(bool useLauncherEndpoint)
{
    m_useLauncherEndpoint = useLauncherEndpoint;

    const auto uhs = m_data->mojangservicesToken.extra["uhs"].toString();
    const auto xToken = m_data->mojangservicesToken.token;
    if (uhs.isEmpty() || xToken.isEmpty()) {
        emit finished(AccountTaskState::STATE_FAILED_SOFT,
                      tr("Missing Xbox authorization data. The Xbox Live step did not return a valid token."));
        return;
    }

    const QString identityToken = buildIdentityToken(uhs, xToken);
    QJsonObject body;
    QUrl url;
    if (m_useLauncherEndpoint) {
        url = QUrl("https://api.minecraftservices.com/launcher/login");
        body["xtoken"] = identityToken;
        body["platform"] = "PC_LAUNCHER";
    } else {
        url = QUrl("https://api.minecraftservices.com/authentication/login_with_xbox");
        body["identityToken"] = identityToken;
    }

    const QByteArray requestBody = QJsonDocument(body).toJson(QJsonDocument::Compact);
    auto headers = QList<Net::HeaderPair>{
        { "Content-Type", "application/json" },
        { "Accept", "application/json" },
    };

    auto [request, response] = Net::Upload::makeByteArray(url, requestBody);
    m_request = request;
    m_request->addHeaderProxy(std::make_unique<Net::RawHeaderProxy>(headers));
    m_request->enableAutoRetry(false);  // avoid duplicating 403 bodies

    m_task.reset(new NetJob("LauncherLoginStep", APPLICATION->network()));
    m_task->setAskRetry(false);
    m_task->addNetAction(m_request);

    connect(m_task.get(), &Task::finished, this, [this, response] { onRequestDone(response); });

    m_task->start();
    qDebug() << "Getting Minecraft access token via" << url.toString();
}

void LauncherLoginStep::onRequestDone(QByteArray* response)
{
    qCDebug(authCredentials()) << *response;

    if (m_request->error() != QNetworkReply::NoError) {
        const int status = m_request->replyStatusCode();
        const QString apiError = parseMinecraftAuthError(*response);

        if (status == 403 && !m_triedFallback) {
            m_triedFallback = true;
            const bool nextUseLauncher = !m_useLauncherEndpoint;
            qWarning() << "Minecraft login returned 403 via"
                       << (m_useLauncherEndpoint ? "launcher/login" : "login_with_xbox") << "- retrying with"
                       << (nextUseLauncher ? "launcher/login" : "login_with_xbox");
            // Keep the first/more specific API error for the final message if the fallback also fails.
            m_firstApiError = apiError;
            startRequest(nextUseLauncher);
            return;
        }

        qWarning() << "Reply error:" << m_request->error() << "HTTP" << status << apiError;

        const QString shownError = !m_firstApiError.isEmpty() ? m_firstApiError : apiError;

        QString message = tr("Failed to get Minecraft access token");
        if (!shownError.isEmpty()) {
            message += QStringLiteral(": %1").arg(shownError);
        } else {
            message += QStringLiteral(": %1").arg(m_request->errorString());
        }

        if (status == 403 || shownError.contains(QStringLiteral("Invalid app registration"), Qt::CaseInsensitive)) {
            message += QStringLiteral("\n\n%1").arg(
                tr("This is NOT blocked by XylarJava.\n"
                   "Microsoft/Mojang rejected your Azure app for Minecraft Services.\n\n"
                   "Your client ID works for Microsoft/Xbox login, but Minecraft requires a "
                   "separately approved App ID.\n\n"
                   "Apply here (can take days/weeks):\n"
                   "https://aka.ms/mce-reviewappid\n\n"
                   "Info: https://aka.ms/AppRegInfo\n\n"
                   "Until Microsoft approves App ID %1, no launcher can make this custom client ID work.")
                    .arg(APPLICATION->getMSAClientID()));
        }

        if (Net::isApplicationError(m_request->error())) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, message);
        } else {
            emit finished(AccountTaskState::STATE_OFFLINE, message);
        }
        return;
    }

    if (!Parsers::parseMojangResponse(*response, m_data->yggdrasilToken)) {
        qWarning() << "Could not parse Minecraft login response...";
        emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Failed to parse the Minecraft access token response."));
        return;
    }
    emit finished(AccountTaskState::STATE_WORKING, tr("Got Minecraft access token"));
}
