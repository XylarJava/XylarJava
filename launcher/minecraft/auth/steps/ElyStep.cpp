#include "ElyStep.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QUuid>
#include "Application.h"

ElyStep::ElyStep(AccountData* data, bool refresh, QNetworkAccessManager* network)
    : AuthStep(data), m_refresh(refresh), m_network(network ? network : APPLICATION->network())
{}

QString ElyStep::describe()
{
    return tr("Signing in to Ely.by");
}
void ElyStep::abort()
{
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
    }
}

void ElyStep::perform()
{
    auto& token = m_data->yggdrasilToken;
    QString client = token.extra.value("clientToken").toString();
    if (client.isEmpty())
        client = QUuid::createUuid().toString(QUuid::Id128);
    QJsonObject body{ { "clientToken", client }, { "requestUser", true } };
    if (m_refresh)
        body["accessToken"] = token.token;
    else {
        body["username"] = m_data->elyLoginName;
        body["password"] = m_data->elyPassword;
    }
    // Credentials are transient and are never written to accounts.json or sent to Xylar services.
    m_data->elyPassword.clear();
    QNetworkRequest request(QUrl(QStringLiteral("https://authserver.ely.by/auth/") + (m_refresh ? "refresh" : "authenticate")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(30000);
    auto* reply = m_network->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, client] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto json = QJsonDocument::fromJson(reply->readAll()).object();
        if (status != 200 || reply->error() != QNetworkReply::NoError) {
            const auto state =
                (status == 400 || status == 401 || status == 403) ? AccountTaskState::STATE_FAILED_HARD : AccountTaskState::STATE_OFFLINE;
            QString message = json.value("errorMessage").toString();
            if (message.contains("two factor", Qt::CaseInsensitive))
                message = tr("This Ely.by account requires a two-factor code. Enter the current code and try again.");
            emit finished(state, message.isEmpty() ? tr("Could not contact Ely.by (HTTP %1).").arg(status) : message);
            return;
        }
        const auto selected = json.value("selectedProfile").toObject();
        QString wireId = selected.value("id").toString();
        if (wireId.size() == 32) {
            wireId.insert(8, '-');
            wireId.insert(13, '-');
            wireId.insert(18, '-');
            wireId.insert(23, '-');
        }
        const QUuid parsedId(wireId);
        const QString id = parsedId.toString(QUuid::Id128);
        const QString name = selected.value("name").toString();
        const QString access = json.value("accessToken").toString();
        if (parsedId.isNull() || name.isEmpty() || access.isEmpty() ||
            (m_refresh && !m_data->minecraftProfile.id.isEmpty() && id != m_data->minecraftProfile.id)) {
            emit finished(AccountTaskState::STATE_FAILED_SOFT, tr("Ely.by returned an invalid or different account profile."));
            return;
        }
        auto& token = m_data->yggdrasilToken;
        token.token = access;
        token.extra["clientToken"] = client;
        token.validity = Validity::Certain;
        token.issueInstant = QDateTime::currentDateTimeUtc();
        token.notAfter = token.issueInstant.addSecs(86400);
        m_data->minecraftProfile.id = id;
        m_data->minecraftProfile.name = name;
        m_data->minecraftProfile.validity = Validity::Certain;
        m_data->minecraftEntitlement = { true, true, Validity::Certain };
        profile();
    });
}

void ElyStep::profile()
{
    QNetworkRequest request(QUrl("https://authserver.ely.by/session/profile/" + m_data->minecraftProfile.id));
    request.setTransferTimeout(30000);
    auto* reply = m_network->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        const auto result = QJsonDocument::fromJson(reply->readAll()).object();
        // Skin service outages must not invalidate an otherwise valid account.
        if (reply->error() == QNetworkReply::NoError)
            skin(result);
        emit finished(AccountTaskState::STATE_WORKING, tr("Ely.by account verified"));
    });
}

void ElyStep::skin(const QJsonObject& profile)
{
    for (const auto& value : profile.value("properties").toArray()) {
        const auto property = value.toObject();
        if (property.value("name") != "textures")
            continue;
        const auto textures = QJsonDocument::fromJson(QByteArray::fromBase64(property.value("value").toString().toLatin1())).object();
        const auto skin = textures.value("textures").toObject().value("SKIN").toObject();
        QUrl url(skin.value("url").toString());
        const auto host = url.host().toLower();
        if (host != "ely.by" && !host.endsWith(".ely.by") && host != "textures.minecraft.net")
            continue;
        url.setScheme("https");
        if (!url.userInfo().isEmpty())
            continue;
        m_data->minecraftProfile.skin.url = url.toString();
        m_data->minecraftProfile.skin.variant = skin.value("metadata").toObject().value("model").toString("classic");
    }
}
