#include "ProfileClient.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QPointer>
#include <QRegularExpression>
#include <QUrl>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceList.h"
#include "SocialClient.h"
#include "XylarService.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/SettingsObject.h"

namespace {
const QString kService = QStringLiteral("XylarJava Profiles");

QList<QPair<QString, qint64>> timesFrom(const QJsonArray& array, const char* key)
{
    QList<QPair<QString, qint64>> out;
    for (const auto& value : array) {
        const QJsonObject row = value.toObject();
        out.append({ row.value(QLatin1String(key)).toString(), row.value("seconds").toInteger() });
    }
    return out;
}
}  // namespace

ProfileClient::ProfileClient(QNetworkAccessManager* network, SocialClient* social, QObject* parent)
    : QObject(parent), m_network(network), m_social(social)
{
    connect(m_social, &SocialClient::changed, this, &ProfileClient::sendBaselineIfNeeded);
}

void ProfileClient::setBackendUrl(const QString& url)
{
    m_backendUrl = XylarService::baseUrl(url);
    sendBaselineIfNeeded();
}

void ProfileClient::fetch(const QString& name, QObject* context, Done done)
{
    const QString url = m_backendUrl + QStringLiteral("/v1/profiles/") + QString::fromLatin1(QUrl::toPercentEncoding(name));
    auto reply = XylarService::call(m_network, url, "GET", {}, context, [done](int status, const QJsonObject& body) {
        if (status != 200)
            return done(false, {}, XylarService::messageFor(kService, status, body));
        Profile profile;
        profile.uuid = body.value("uuid").toString();
        profile.name = body.value("name").toString();
        profile.memberSince = body.value("memberSince").toInteger();
        profile.playtime = body.value("playtime").toInteger();
        profile.lastPlayedAt = body.value("lastPlayedAt").toInteger();
        profile.lastVersion = body.value("lastVersion").toString();
        profile.favoriteVersion = body.value("favoriteVersion").toString();
        profile.favoriteServer = body.value("favoriteServer").toString();
        profile.versions = timesFrom(body.value("versions").toArray(), "version");
        profile.servers = timesFrom(body.value("servers").toArray(), "server");
        done(true, profile, QString());
    });
    if (!reply)
        done(false, {}, tr("Pick an account first: profiles belong to your XylarJava Social player."));
}

void ProfileClient::sendBaselineIfNeeded()
{
    // The time played so far lives in the instances of this install, so it's sent once per
    // install, for the first player signed in here.
    auto settings = APPLICATION->settings();
    if (m_sendingBaseline || m_backendUrl.isEmpty() || !m_social->ready() || settings->get("XylarProfileBaselineSent").toBool())
        return;

    static const QRegularExpression versionPattern(QStringLiteral("^[A-Za-z0-9 ._+-]{1,32}$"));
    QHash<QString, qint64> perVersion;
    qint64 lastPlayed = 0;
    auto instances = APPLICATION->instances();
    for (int i = 0; i < instances->count(); i++) {
        auto minecraft = dynamic_cast<MinecraftInstance*>(instances->at(i));
        if (!minecraft)
            continue;
        lastPlayed = std::max<qint64>(lastPlayed, minecraft->lastLaunch() / 1000);
        const qint64 seconds = minecraft->totalTimePlayed();
        auto profile = minecraft->getPackProfile();
        QString version = profile ? profile->getComponentVersion(QStringLiteral("net.minecraft")) : QString();
        // Instances nobody opened this session haven't loaded their components yet.
        if (profile && version.isEmpty() && seconds > 0) {
            profile->reload(Net::Mode::Offline);
            version = profile->getComponentVersion(QStringLiteral("net.minecraft"));
        }
        if (seconds > 0 && versionPattern.match(version).hasMatch())
            perVersion[version] += seconds;
    }
    QJsonObject versions;
    for (auto it = perVersion.cbegin(); it != perVersion.cend(); ++it)
        versions.insert(it.key(), it.value());
    QJsonObject body{ { "versions", versions } };
    if (lastPlayed > 0 && lastPlayed <= QDateTime::currentSecsSinceEpoch())
        body.insert("lastPlayedAt", lastPlayed);

    m_sendingBaseline = true;
    auto reply = XylarService::call(m_network, m_backendUrl + QStringLiteral("/v1/profiles/me/baseline"), "POST",
                                    QJsonDocument(body).toJson(QJsonDocument::Compact), this, [this](int status, const QJsonObject& reply) {
                                        m_sendingBaseline = false;
                                        // 200 means this player already had one: nothing more to send either way.
                                        if (status == 200 || status == 201)
                                            APPLICATION->settings()->set("XylarProfileBaselineSent", true);
                                        else
                                            qWarning() << "Profiles: the time played so far wasn't sent:"
                                                       << XylarService::messageFor(kService, status, reply);
                                    });
    if (!reply)
        m_sendingBaseline = false;
}
