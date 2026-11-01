#include "MojangSkinFetcher.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace {
constexpr qint64 kDiskCacheSecs = 24 * 60 * 60;

QNetworkRequest jsonRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "XylarJava");
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    return request;
}

QString plainUuid(QString uuid)
{
    return uuid.remove(QLatin1Char('-')).toLower();
}
}  // namespace

MojangSkinFetcher::MojangSkinFetcher(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network) {}

QString MojangSkinFetcher::keyForUuid(const QString& uuid)
{
    return QStringLiteral("uuid:") + plainUuid(uuid);
}

QString MojangSkinFetcher::keyForName(const QString& name)
{
    return QStringLiteral("name:") + name.toLower();
}

QString MojangSkinFetcher::diskPath(const QString& key) const
{
    QString file = key;
    file.replace(QLatin1Char(':'), QLatin1Char('_'));
    file.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_\\-]")), QStringLiteral("_"));
    const QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation)).filePath(QStringLiteral("skins"));
    return QDir(dir).filePath(file + QStringLiteral(".png"));
}

bool MojangSkinFetcher::loadFromDisk(const QString& key, Skin* out) const
{
    const QString path = diskPath(key);
    QImage image;
    if (!image.load(path, "PNG"))
        return false;
    out->texture = image;
    out->slim = QFileInfo::exists(path + QStringLiteral(".slim"));
    return true;
}

bool MojangSkinFetcher::cached(const QString& key, Skin* out) const
{
    auto it = m_cache.constFind(key);
    if (it != m_cache.constEnd()) {
        *out = it.value();
        return true;
    }
    return loadFromDisk(key, out);
}

void MojangSkinFetcher::fetchByUuid(const QString& uuid)
{
    const QString key = keyForUuid(uuid);
    if (m_pending.contains(key) || m_cache.contains(key))
        return;
    const QFileInfo disk(diskPath(key));
    if (disk.exists() && disk.lastModified().secsTo(QDateTime::currentDateTime()) < kDiskCacheSecs) {
        Skin skin;
        if (loadFromDisk(key, &skin)) {
            m_cache.insert(key, skin);
            emit finished(key, skin.texture, skin.slim);
            return;
        }
    }
    m_pending.insert(key, true);
    fetchProfile(key, plainUuid(uuid));
}

void MojangSkinFetcher::fetchByName(const QString& name)
{
    const QString key = keyForName(name);
    if (name.isEmpty() || m_pending.contains(key) || m_cache.contains(key))
        return;
    const QFileInfo disk(diskPath(key));
    if (disk.exists() && disk.lastModified().secsTo(QDateTime::currentDateTime()) < kDiskCacheSecs) {
        Skin skin;
        if (loadFromDisk(key, &skin)) {
            m_cache.insert(key, skin);
            emit finished(key, skin.texture, skin.slim);
            return;
        }
    }
    m_pending.insert(key, true);

    const QUrl url(QStringLiteral("https://api.mojang.com/users/profiles/minecraft/%1").arg(QString::fromLatin1(QUrl::toPercentEncoding(name))));
    QNetworkReply* reply = m_network->get(jsonRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QJsonObject profile = QJsonDocument::fromJson(reply->readAll()).object();
        const QString id = profile.value(QStringLiteral("id")).toString();
        if (reply->error() != QNetworkReply::NoError || id.isEmpty()) {
            // 404/204: no Mojang account with this name. Other errors: try again next time.
            if (status == 404 || status == 204 || (reply->error() == QNetworkReply::NoError && id.isEmpty()))
                done(key, QImage(), false);
            else
                failed(key);
            return;
        }
        fetchProfile(key, id);
    });
}

void MojangSkinFetcher::fetchProfile(const QString& key, const QString& uuid)
{
    const QUrl url(QStringLiteral("https://sessionserver.mojang.com/session/minecraft/profile/%1").arg(uuid));
    QNetworkReply* reply = m_network->get(jsonRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, key] {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            failed(key);
            return;
        }
        const QJsonObject profile = QJsonDocument::fromJson(reply->readAll()).object();
        for (const QJsonValue& value : profile.value(QStringLiteral("properties")).toArray()) {
            const QJsonObject property = value.toObject();
            if (property.value(QStringLiteral("name")).toString() != QLatin1String("textures"))
                continue;
            const QByteArray decoded = QByteArray::fromBase64(property.value(QStringLiteral("value")).toString().toLatin1());
            const QJsonObject skin =
                QJsonDocument::fromJson(decoded).object().value(QStringLiteral("textures")).toObject().value(QStringLiteral("SKIN")).toObject();
            const QString url = skin.value(QStringLiteral("url")).toString();
            const bool slim = skin.value(QStringLiteral("metadata")).toObject().value(QStringLiteral("model")).toString() ==
                              QLatin1String("slim");
            if (!url.isEmpty()) {
                fetchTexture(key, url, slim);
                return;
            }
        }
        // Profile exists but uses the default skin.
        done(key, QImage(), false);
    });
}

void MojangSkinFetcher::fetchTexture(const QString& key, const QString& url, bool slim)
{
    QUrl textureUrl(url);
    textureUrl.setScheme(QStringLiteral("https"));
    QNetworkRequest request(textureUrl);
    request.setRawHeader("User-Agent", "XylarJava");
    request.setTransferTimeout(15000);
    QNetworkReply* reply = m_network->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, key, slim] {
        reply->deleteLater();
        QImage texture;
        if (reply->error() != QNetworkReply::NoError || !texture.loadFromData(reply->readAll(), "PNG")) {
            failed(key);
            return;
        }
        done(key, texture, slim);
    });
}

void MojangSkinFetcher::failed(const QString& key)
{
    // Network trouble: remember it for this session (keeping any skin cached on disk)
    // so callers reacting to finished() can't trigger a request loop.
    m_pending.remove(key);
    Skin skin;
    loadFromDisk(key, &skin);
    m_cache.insert(key, skin);
    emit finished(key, skin.texture, skin.slim);
}

void MojangSkinFetcher::done(const QString& key, const QImage& texture, bool slim)
{
    m_pending.remove(key);
    m_cache.insert(key, Skin{ texture, slim });

    const QString path = diskPath(key);
    if (!texture.isNull()) {
        QDir().mkpath(QFileInfo(path).absolutePath());
        texture.save(path, "PNG");
        if (slim) {
            QFile marker(path + QStringLiteral(".slim"));
            if (marker.open(QIODevice::WriteOnly))
                marker.close();
        } else {
            QFile::remove(path + QStringLiteral(".slim"));
        }
    } else {
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".slim"));
    }
    emit finished(key, texture, slim);
}
