#include "MSAStep.h"

#include <QAbstractOAuth2>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QOAuthHttpServerReplyHandler>
#include <QOAuthOobReplyHandler>

#include "Application.h"
#include "BuildConfig.h"
#include "FileSystem.h"

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

bool isSchemeHandlerRegistered()
{
#ifdef Q_OS_LINUX
    QProcess process;
    process.start("xdg-mime", { "query", "default", "x-scheme-handler/" + BuildConfig.LAUNCHER_APP_BINARY_NAME });
    process.waitForFinished();
    QString output = process.readAllStandardOutput().trimmed();

    return output.contains(APPLICATION->desktopFileName());

#elif defined(Q_OS_WIN)
    QString regPath = QString("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(BuildConfig.LAUNCHER_APP_BINARY_NAME);
    QSettings settings(regPath, QSettings::NativeFormat);

    const QString registeredRunCommand = settings.value("shell/open/command/.").toString().replace("\\", "/");
    return registeredRunCommand.contains(QCoreApplication::applicationFilePath());
#endif
    return true;
}

class CustomOAuthOobReplyHandler : public QOAuthOobReplyHandler {
    Q_OBJECT

   public:
    explicit CustomOAuthOobReplyHandler(QObject* parent = nullptr) : QOAuthOobReplyHandler(parent)
    {
        connect(APPLICATION, &Application::oauthReplyRecieved, this, &QOAuthOobReplyHandler::callbackReceived);
    }
    ~CustomOAuthOobReplyHandler() override
    {
        disconnect(APPLICATION, &Application::oauthReplyRecieved, this, &QOAuthOobReplyHandler::callbackReceived);
    }
    QString callback() const override { return BuildConfig.LAUNCHER_APP_BINARY_NAME + "://oauth/microsoft"; }

   protected:
    void networkReplyFinished(QNetworkReply* reply) override
    {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "OAuth2 request failed:" << reply->readAll();
        }

        QOAuthOobReplyHandler::networkReplyFinished(reply);
    }
};

class LoggingOAuthHttpServerReplyHandler final : public QOAuthHttpServerReplyHandler {
    Q_OBJECT

   public:
    explicit LoggingOAuthHttpServerReplyHandler(QObject* parent = nullptr) : QOAuthHttpServerReplyHandler(parent) {}

   protected:
    void networkReplyFinished(QNetworkReply* reply) override
    {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "OAuth2 request failed:" << reply->readAll();
        }

        QOAuthHttpServerReplyHandler::networkReplyFinished(reply);
    }
};

MSAStep::MSAStep(AccountData* data, bool silent) : AuthStep(data), m_silent(silent)
{
    m_clientId = APPLICATION->getMSAClientID();

    // Always use localhost HTTP callback so it matches Azure "Mobile and desktop" public-client
    // redirect URIs (http://127.0.0.1 / http://localhost). Avoid custom scheme + device-code.
    auto replyHandler = new LoggingOAuthHttpServerReplyHandler(this);
    replyHandler->setCallbackText(QString(R"XXX(
    <noscript>
      <meta http-equiv="Refresh" content="0; URL=%1" />
    </noscript>
    Login Successful, you can close this tab and return to XylarJava.
    <script>
      window.location.replace("%1");
    </script>
    )XXX")
                                      .arg(BuildConfig.LOGIN_CALLBACK_URL));
    m_oauth2.setReplyHandler(replyHandler);
    m_oauth2.setAuthorizationUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/authorize"));
    m_oauth2.setAccessTokenUrl(QUrl("https://login.microsoftonline.com/consumers/oauth2/v2.0/token"));
    m_oauth2.setScope("XboxLive.signin XboxLive.offline_access");
    m_oauth2.setClientIdentifier(m_clientId);
    m_oauth2.setNetworkAccessManager(APPLICATION->network());

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::granted, this, [this] {
        m_data->msaClientID = m_oauth2.clientIdentifier();
        m_data->msaToken.issueInstant = QDateTime::currentDateTimeUtc();
        m_data->msaToken.notAfter = m_oauth2.expirationAt();
        m_data->msaToken.extra = m_oauth2.extraTokens();
        m_data->msaToken.refresh_token = m_oauth2.refreshToken();
        m_data->msaToken.token = m_oauth2.token();
        emit finished(AccountTaskState::STATE_WORKING, tr("Got MSA token"));
    });
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::authorizeWithBrowser, this, &MSAStep::authorizeWithBrowser);
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::requestFailed, this, [this, silent](const QAbstractOAuth2::Error err) {
        auto state = AccountTaskState::STATE_FAILED_HARD;
        if (m_oauth2.status() == QAbstractOAuth::Status::Granted || silent) {
            if (err == QAbstractOAuth2::Error::NetworkError) {
                state = AccountTaskState::STATE_OFFLINE;
            } else {
                state = AccountTaskState::STATE_FAILED_SOFT;
            }
        }
        auto message = tr("Microsoft user authentication failed.");
        if (silent) {
            message = tr("Failed to refresh token.");
        }
        qWarning() << message;
        emit finished(state, message);
    });
    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::error, this,
            [this](const QString& error, const QString& errorDescription, const QUrl& uri) {
                qWarning() << "Failed to login because" << error << errorDescription;
                emit finished(AccountTaskState::STATE_FAILED_HARD, errorDescription);
            });

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::extraTokensChanged, this,
            [this](const QVariantMap& tokens) { m_data->msaToken.extra = tokens; });

    connect(&m_oauth2, &QOAuth2AuthorizationCodeFlow::clientIdentifierChanged, this,
            [this](const QString& clientIdentifier) { m_data->msaClientID = clientIdentifier; });
}

QString MSAStep::describe()
{
    return tr("Logging in with Microsoft account.");
}

void MSAStep::perform()
{
    if (m_silent) {
        if (m_data->msaClientID != m_clientId) {
            emit finished(AccountTaskState::STATE_DISABLED,
                          tr("Microsoft user authentication failed - client identification has changed."));
            return;
        }
        if (m_data->msaToken.refresh_token.isEmpty()) {
            emit finished(AccountTaskState::STATE_DISABLED, tr("Microsoft user authentication failed - refresh token is empty."));
            return;
        }
        m_oauth2.setRefreshToken(m_data->msaToken.refresh_token);
        m_oauth2.refreshAccessToken();
    } else {
        m_oauth2.setModifyParametersFunction(
            [](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant>* map) { map->insert("prompt", "select_account"); });

        *m_data = AccountData();
        m_data->msaClientID = m_clientId;
        m_oauth2.grant();
    }
}

#include "MSAStep.moc"
