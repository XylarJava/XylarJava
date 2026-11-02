#include "ServerSoftware.h"

#include <QJsonArray>
#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <memory>

#include "BuildConfig.h"

namespace ServerSoftware {

namespace {

const char* MOJANG_MANIFEST = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
const char* FABRIC_META = "https://meta.fabricmc.net/v2/versions/";
const char* PAPER_FILL = "https://fill.papermc.io/v3/projects/paper";

using JsonCallback = std::function<void(const QJsonDocument& doc, const QString& error)>;

void getJson(QNetworkAccessManager* network, const QUrl& url, QObject* context, JsonCallback done)
{
    QNetworkRequest request(url);
    // PaperMC asks for a User-Agent that names the software and a contact.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("%1 (https://github.com/XylarJava/XylarJava)").arg(BuildConfig.USER_AGENT));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, context, [reply, done] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            done(QJsonDocument(), reply->errorString());
            return;
        }
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            done(QJsonDocument(), QObject::tr("The server list couldn't be read (%1).").arg(parseError.errorString()));
            return;
        }
        done(doc, QString());
    });
}

/// "1.21.4" > "1.21" > "1.20.6"; non-numeric parts compare as text.
bool newerThan(const QString& a, const QString& b)
{
    const QStringList pa = a.split('.');
    const QStringList pb = b.split('.');
    for (int i = 0; i < qMax(pa.size(), pb.size()); i++) {
        bool okA = false, okB = false;
        const int na = i < pa.size() ? pa.at(i).toInt(&okA) : 0;
        const int nb = i < pb.size() ? pb.at(i).toInt(&okB) : 0;
        if (okA && okB && na != nb)
            return na > nb;
        if (!(okA && okB) && i < pa.size() && i < pb.size() && pa.at(i) != pb.at(i))
            return pa.at(i) > pb.at(i);
    }
    return pa.size() > pb.size();
}

}  // namespace

QString id(Kind kind)
{
    switch (kind) {
        case Kind::Fabric:
            return QStringLiteral("fabric");
        case Kind::Paper:
            return QStringLiteral("paper");
        case Kind::Vanilla:
            break;
    }
    return QStringLiteral("vanilla");
}

Kind fromId(const QString& text)
{
    if (text == QLatin1String("fabric"))
        return Kind::Fabric;
    if (text == QLatin1String("paper"))
        return Kind::Paper;
    return Kind::Vanilla;
}

QString displayName(Kind kind)
{
    switch (kind) {
        case Kind::Fabric:
            return QStringLiteral("Fabric");
        case Kind::Paper:
            return QStringLiteral("Paper");
        case Kind::Vanilla:
            break;
    }
    return QStringLiteral("Vanilla");
}

QString addonsFolder(Kind kind)
{
    switch (kind) {
        case Kind::Fabric:
            return QStringLiteral("mods");
        case Kind::Paper:
            return QStringLiteral("plugins");
        case Kind::Vanilla:
            break;
    }
    return QString();
}

int requiredJava(const QString& minecraftVersion)
{
    const QStringList parts = minecraftVersion.split('.');
    const int minor = parts.size() > 1 ? parts.at(1).toInt() : 0;
    const int patch = parts.size() > 2 ? parts.at(2).toInt() : 0;
    if (minor > 20 || (minor == 20 && patch >= 5))
        return 21;
    if (minor >= 18)
        return 17;
    if (minor == 17)
        return 16;
    return 8;
}

void fetchVersions(QNetworkAccessManager* network, Kind kind, QObject* context, VersionsCallback done)
{
    if (kind == Kind::Vanilla) {
        getJson(network, QUrl(QString::fromLatin1(MOJANG_MANIFEST)), context, [done](const QJsonDocument& doc, const QString& error) {
            if (!error.isEmpty())
                return done({}, error);
            QStringList versions;
            for (const QJsonValue& value : doc.object().value("versions").toArray()) {
                const QJsonObject v = value.toObject();
                if (v.value("type").toString() == QLatin1String("release"))
                    versions << v.value("id").toString();
            }
            done(versions, QString());
        });
    } else if (kind == Kind::Fabric) {
        getJson(network, QUrl(QString::fromLatin1(FABRIC_META) + "game"), context, [done](const QJsonDocument& doc, const QString& error) {
            if (!error.isEmpty())
                return done({}, error);
            QStringList versions;
            for (const QJsonValue& value : doc.array()) {
                const QJsonObject v = value.toObject();
                if (v.value("stable").toBool())
                    versions << v.value("version").toString();
            }
            done(versions, QString());
        });
    } else {
        getJson(network, QUrl(QString::fromLatin1(PAPER_FILL)), context, [done](const QJsonDocument& doc, const QString& error) {
            if (!error.isEmpty())
                return done({}, error);
            QStringList versions;
            const QJsonObject groups = doc.object().value("versions").toObject();
            static const QRegularExpression release(QStringLiteral("^\\d+(\\.\\d+)+$"));
            for (auto it = groups.begin(); it != groups.end(); ++it) {
                for (const QJsonValue& value : it.value().toArray()) {
                    const QString v = value.toString();
                    if (release.match(v).hasMatch())
                        versions << v;
                }
            }
            std::sort(versions.begin(), versions.end(), newerThan);
            versions.removeDuplicates();
            done(versions, QString());
        });
    }
}

void resolveJar(QNetworkAccessManager* network, Kind kind, const QString& version, QObject* context, UrlCallback done)
{
    if (kind == Kind::Vanilla) {
        getJson(network, QUrl(QString::fromLatin1(MOJANG_MANIFEST)), context,
                [network, version, context, done](const QJsonDocument& doc, const QString& error) {
                    if (!error.isEmpty())
                        return done(QUrl(), error);
                    QString versionUrl;
                    for (const QJsonValue& value : doc.object().value("versions").toArray()) {
                        if (value.toObject().value("id").toString() == version)
                            versionUrl = value.toObject().value("url").toString();
                    }
                    if (versionUrl.isEmpty())
                        return done(QUrl(), QObject::tr("Minecraft %1 wasn't found.").arg(version));
                    getJson(network, QUrl(versionUrl), context, [version, done](const QJsonDocument& versionDoc, const QString& versionError) {
                        if (!versionError.isEmpty())
                            return done(QUrl(), versionError);
                        const QString url =
                            versionDoc.object().value("downloads").toObject().value("server").toObject().value("url").toString();
                        if (url.isEmpty())
                            return done(QUrl(), QObject::tr("Minecraft %1 has no official server download.").arg(version));
                        done(QUrl(url), QString());
                    });
                });
    } else if (kind == Kind::Fabric) {
        // The launcher jar for (game, latest stable loader, latest stable installer) downloads the rest itself.
        auto loader = std::make_shared<QString>();
        getJson(network, QUrl(QString::fromLatin1(FABRIC_META) + "loader"), context,
                [network, version, context, done, loader](const QJsonDocument& doc, const QString& error) {
                    if (!error.isEmpty())
                        return done(QUrl(), error);
                    for (const QJsonValue& value : doc.array()) {
                        if (value.toObject().value("stable").toBool()) {
                            *loader = value.toObject().value("version").toString();
                            break;
                        }
                    }
                    if (loader->isEmpty())
                        return done(QUrl(), QObject::tr("No stable Fabric loader was found."));
                    getJson(network, QUrl(QString::fromLatin1(FABRIC_META) + "installer"), context,
                            [version, done, loader](const QJsonDocument& installers, const QString& installerError) {
                                if (!installerError.isEmpty())
                                    return done(QUrl(), installerError);
                                QString installer;
                                for (const QJsonValue& value : installers.array()) {
                                    if (value.toObject().value("stable").toBool()) {
                                        installer = value.toObject().value("version").toString();
                                        break;
                                    }
                                }
                                if (installer.isEmpty())
                                    return done(QUrl(), QObject::tr("No stable Fabric installer was found."));
                                done(QUrl(QStringLiteral("%1loader/%2/%3/%4/server/jar")
                                              .arg(QString::fromLatin1(FABRIC_META), version, *loader, installer)),
                                     QString());
                            });
                });
    } else {
        getJson(network, QUrl(QStringLiteral("%1/versions/%2/builds").arg(QString::fromLatin1(PAPER_FILL), version)), context,
                [version, done](const QJsonDocument& doc, const QString& error) {
                    if (!error.isEmpty())
                        return done(QUrl(), error);
                    QString stable, any;
                    for (const QJsonValue& value : doc.array()) {
                        const QJsonObject build = value.toObject();
                        const QString url = build.value("downloads").toObject().value("server:default").toObject().value("url").toString();
                        if (url.isEmpty())
                            continue;
                        if (any.isEmpty())
                            any = url;
                        if (stable.isEmpty() && build.value("channel").toString() == QLatin1String("STABLE"))
                            stable = url;
                    }
                    const QString pick = stable.isEmpty() ? any : stable;
                    if (pick.isEmpty())
                        return done(QUrl(), QObject::tr("Paper has no build for %1 yet.").arg(version));
                    done(QUrl(pick), QString());
                });
    }
}

}  // namespace ServerSoftware
