#include "XylarSkinClient.h"

#include <QCryptographicHash>
#include <QJsonArray>
#include <QRegularExpression>
#include <QSaveFile>
#include <QUuid>

#include "minecraft/auth/AccountTokenCipher.h"

XylarSkinClient::XylarSkinClient(QObject* parent, QNetworkAccessManager* network)
    : QObject(parent), m_nam(network ? network : new QNetworkAccessManager(this)), m_cacheDir(defaultCacheDir())
{
    QDir().mkpath(m_cacheDir);
}

void XylarSkinClient::setBackendUrl(const QString& url)
{
    m_backendUrl = url.trimmed();
    while (m_backendUrl.endsWith('/')) {
        m_backendUrl.chop(1);
    }
}

QString XylarSkinClient::backendUrl() const
{
    return m_backendUrl;
}

void XylarSkinClient::setCapeBackendUrl(const QString& url)
{
    m_capeBackendUrl = url.trimmed();
    while (m_capeBackendUrl.endsWith('/')) {
        m_capeBackendUrl.chop(1);
    }
}

QString XylarSkinClient::capeBackendUrl() const
{
    return m_capeBackendUrl;
}

void XylarSkinClient::setAuthToken(const QString& token)
{
    m_authToken = token.trimmed();
}

QString XylarSkinClient::authToken() const
{
    return m_authToken;
}

void XylarSkinClient::setCacheDir(const QString& dir)
{
    m_cacheDir = dir.trimmed();
    QDir().mkpath(m_cacheDir);
}

QString XylarSkinClient::cacheDir() const
{
    return m_cacheDir;
}

QString XylarSkinClient::getCachedSkinPath(const QString& uuid) const
{
    return QDir(m_cacheDir).filePath(uuid + ".png");
}

bool XylarSkinClient::hasCachedSkin(const QString& uuid) const
{
    return QFile::exists(getCachedSkinPath(uuid));
}

QByteArray XylarSkinClient::loadCachedSkin(const QString& uuid) const
{
    QFile f(getCachedSkinPath(uuid));
    if (f.open(QIODevice::ReadOnly)) {
        return f.readAll();
    }
    return QByteArray();
}

bool XylarSkinClient::saveCachedSkin(const QString& uuid, const QByteArray& data)
{
    QDir().mkpath(m_cacheDir);
    QSaveFile f(getCachedSkinPath(uuid));
    if (f.open(QIODevice::WriteOnly)) {
        return f.write(data) == data.size() && f.commit();
    }
    return false;
}

bool XylarSkinClient::deleteCachedSkin(const QString& uuid)
{
    QString path = getCachedSkinPath(uuid);
    if (QFile::exists(path)) {
        return QFile::remove(path);
    }
    return true;
}

QString XylarSkinClient::defaultCacheDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + "/.local/share";
    }
    return QDir(base).filePath("XylarJava/skins");
}

QString XylarSkinClient::computeOfflineUUID(const QString& username)
{
    auto input = QString("OfflinePlayer:%1").arg(username).toUtf8();
    QByteArray digest = QCryptographicHash::hash(input, QCryptographicHash::Md5);
    if (digest.size() >= 16) {
        digest[6] = (digest[6] & 0x0f) | 0x30;  // version 3
        digest[8] = (digest[8] & 0x3f) | 0x80;  // IETF variant
    }
    QUuid uuid = QUuid::fromRfc4122(digest);
    return uuid.toString(QUuid::WithoutBraces);
}

bool XylarSkinClient::validateSkinPng(const QString& filePath, QString* outError)
{
    QFile file(filePath);
    if (!file.exists()) {
        if (outError)
            *outError = tr("Selected skin file does not exist.");
        return false;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (outError)
            *outError = tr("Cannot read selected skin file: %1").arg(file.errorString());
        return false;
    }
    QByteArray data = file.readAll();
    file.close();
    return validateSkinPngData(data, outError);
}

bool XylarSkinClient::validateSkinPngData(const QByteArray& data, QString* outError)
{
    if (data.size() < 24) {
        if (outError)
            *outError = tr("File is too small to be a valid PNG image.");
        return false;
    }

    // Check PNG signature: 89 50 4E 47 0D 0A 1A 0A
    static const unsigned char pngSig[8] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
    if (memcmp(data.constData(), pngSig, 8) != 0) {
        if (outError)
            *outError = tr("File is not a valid PNG image.");
        return false;
    }

    // Read IHDR chunk width and height (bytes 16..23)
    quint32 width = (uchar(data[16]) << 24) | (uchar(data[17]) << 16) | (uchar(data[18]) << 8) | uchar(data[19]);
    quint32 height = (uchar(data[20]) << 24) | (uchar(data[21]) << 16) | (uchar(data[22]) << 8) | uchar(data[23]);

    if (width != 64 || height != 64) {
        if (outError) {
            *outError = tr("Skin must be 64x64 pixels. Current image is %1x%2.").arg(width).arg(height);
        }
        return false;
    }

    if (data.size() > 1048576) {
        if (outError)
            *outError = tr("Skin file too large (maximum allowed size is 1 MB).");
        return false;
    }

    QImage img;
    if (!img.loadFromData(data, "PNG")) {
        if (outError)
            *outError = tr("Failed to decode PNG image data.");
        return false;
    }

    return true;
}

bool XylarSkinClient::validateCapePngData(const QByteArray& data, QString* outError)
{
    QImage img;
    if (data.size() < 24 || !img.loadFromData(data, "PNG")) {
        if (outError)
            *outError = tr("File is not a valid PNG image.");
        return false;
    }
    const int w = img.width();
    const int h = img.height();
    if (w != h * 2 || w < 64 || w > 2048 || w % 64 != 0) {
        if (outError) {
            *outError = tr("A cape must be 64x32 pixels (or 128x64, 256x128, up to 2048x1024). This image is %1x%2.").arg(w).arg(h);
        }
        return false;
    }
    if (data.size() > 1048576) {
        if (outError)
            *outError = tr("Cape file too large (maximum allowed size is 1 MB).");
        return false;
    }
    return true;
}

QString XylarSkinClient::computeSha256(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

QNetworkRequest XylarSkinClient::makeRequest(const QString& endpoint, bool json, const QString& playerUuid) const
{
    QString urlStr = m_backendUrl + endpoint;
    QNetworkRequest req{ QUrl(urlStr) };

    if (!playerUuid.isEmpty()) {
        const QString key = playerKey(playerUuid);
        if (!key.isEmpty()) {
            req.setRawHeader("X-Player-Key", key.toLatin1());
        }
    }

    if (!m_authToken.isEmpty()) {
        req.setRawHeader("Authorization", ("Bearer " + m_authToken).toUtf8());
    }

    if (json) {
        req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    }

    req.setTransferTimeout(30000);
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    return req;
}

QString XylarSkinClient::parseErrorMessage(QNetworkReply* reply, const QByteArray& responseBody) const
{
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    // Check JSON body for server error message
    if (!responseBody.isEmpty()) {
        QJsonParseError parseErr;
        QJsonDocument doc = QJsonDocument::fromJson(responseBody, &parseErr);
        if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("error") && obj["error"].isString()) {
                return obj["error"].toString();
            }
            if (obj.contains("message") && obj["message"].isString()) {
                return obj["message"].toString();
            }
        }
    }

    switch (statusCode) {
        case 400:
            return tr("Bad request: invalid parameters or image format.");
        case 401:
            return tr("Authentication failed: invalid or missing client token.");
        case 403:
            return tr("Access denied by skin backend.");
        case 404:
            return tr("Player or skin not found on server.");
        case 429:
            return tr("Rate limit exceeded (max 30 requests / 60 seconds). Please wait a moment.");
        case 500:
        case 502:
        case 503:
        case 504:
            return tr("Skin backend server error (HTTP %1).").arg(statusCode);
        default:
            break;
    }

    if (reply->error() == QNetworkReply::ConnectionRefusedError || reply->error() == QNetworkReply::HostNotFoundError ||
        reply->error() == QNetworkReply::TimeoutError || reply->error() == QNetworkReply::RemoteHostClosedError) {
        return tr("Cannot reach skin backend (server offline or network issue).");
    }

    return reply->errorString();
}

void XylarSkinClient::checkHealth()
{
    QNetworkRequest req(QUrl(m_backendUrl + "/api/health"));
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        if (reply->error() != QNetworkReply::NoError) {
            emit healthChecked(false, parseErrorMessage(reply, data));
            return;
        }
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QString svc = doc.object().value("service").toString();
        emit healthChecked(true, svc);
    });
}

void XylarSkinClient::registerPlayer(const QString& username,
                                     const QString& profileId,
                                     const QString& identityToken,
                                     const QString& provider,
                                     const QString& ownedUuid)
{
    if (m_backendUrl.isEmpty()) {
        emit requestFailed("registerPlayer", tr("Profile protection requires a configured Xylar backend."), 0);
        return;
    }
    const QString kind = provider.isEmpty() ? (profileId.isEmpty() ? QStringLiteral("offline") : QStringLiteral("microsoft")) : provider;
    QJsonObject body{ { "username", username }, { "provider", kind } };
    if (kind != "offline") {
        body["profile_id"] = profileId;
        body["identity_token"] = identityToken;
    }
    QNetworkRequest req = makeRequest("/api/identity", true, ownedUuid.isEmpty() ? computeOfflineUUID(username) : ownedUuid);
    req.setTransferTimeout(45000);
    auto* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, username] {
        reply->deleteLater();
        const auto data = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            emit requestFailed("registerPlayer", parseErrorMessage(reply, data), status);
            return;
        }
        const auto result = QJsonDocument::fromJson(data).object();
        const auto player = result["player"].toObject();
        const QString uuid = player["uuid"].toString();
        if (result["identity_version"].toInt() != 2 ||
            !QRegularExpression(QStringLiteral("^[0-9a-fA-F]{8}(-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}$")).match(uuid).hasMatch()) {
            emit requestFailed("registerPlayer", tr("Update the skin backend: it does not support protected usernames yet."), 502);
            return;
        }
        const QString key = result["key"].toString();
        if ((!key.isEmpty() &&
             (!QRegularExpression(QStringLiteral("^[A-Za-z0-9_-]{43}$")).match(key).hasMatch() || !setPlayerKey(uuid, key))) ||
            (key.isEmpty() && playerKey(uuid).isEmpty())) {
            emit requestFailed("registerPlayer", tr("Could not save this profile's access key on this computer."), 0);
            return;
        }
        emit playerRegistered(uuid, player["display_name"].toString(username), player["skin_hash"].toString(),
                              player["skin_model"].toString("classic"));
    });
}

void XylarSkinClient::fetchPlayer(const QString& uuid)
{
    if (m_backendUrl.isEmpty()) {
        emit requestFailed("fetchPlayer", tr("Backend URL is not configured."), 0);
        return;
    }

    QNetworkRequest req = makeRequest("/api/player/" + uuid);
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            QString err = parseErrorMessage(reply, data);
            emit requestFailed("fetchPlayer", err, statusCode);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject player = doc.object()["player"].toObject();
        QString username = player["username"].toString();
        QString skinHash = player["skin_hash"].toString();
        QString skinModel = player["skin_model"].toString("classic");

        emit playerFetched(uuid, username, skinHash, skinModel);
    });
}

void XylarSkinClient::fetchSkinMetadata(const QString& uuid)
{
    if (m_backendUrl.isEmpty()) {
        emit requestFailed("fetchSkinMetadata", tr("Backend URL is not configured."), 0);
        return;
    }

    QNetworkRequest req = makeRequest("/api/skin/" + uuid + "/metadata");
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            QString err = parseErrorMessage(reply, data);
            emit requestFailed("fetchSkinMetadata", err, statusCode);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        bool hasSkin = obj["has_skin"].toBool();
        QString skinHash = obj["skin_hash"].toString();
        QString model = obj["model"].toString("classic");

        emit skinMetadataReceived(uuid, hasSkin, skinHash, model);
    });
}

void XylarSkinClient::downloadSkin(const QString& uuid, const QString& expectedHash, const QString& model)
{
    if (m_backendUrl.isEmpty()) {
        emit requestFailed("downloadSkin", tr("Backend URL is not configured."), 0);
        return;
    }

    QNetworkRequest req = makeRequest("/api/skin/" + uuid, false);
    req.setRawHeader("Accept", "image/png");
    QNetworkReply* reply = m_nam->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid, expectedHash, model]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            QString err = parseErrorMessage(reply, data);
            emit requestFailed("downloadSkin", err, statusCode);
            return;
        }

        // Validate downloaded data
        QString valErr;
        if (!validateSkinPngData(data, &valErr)) {
            emit requestFailed("downloadSkin", tr("Downloaded skin is invalid: %1").arg(valErr), statusCode);
            return;
        }

        // Save to cache
        if (!saveCachedSkin(uuid, data)) {
            emit requestFailed("downloadSkin", tr("Could not save the downloaded skin on this computer."), 0);
            return;
        }

        QString respModel = QString::fromUtf8(reply->rawHeader("X-Skin-Model")).toLower();
        if (respModel.isEmpty()) {
            respModel = model;
        }
        if (respModel != "slim") {
            respModel = "classic";
        }

        QString respHash = QString::fromUtf8(reply->rawHeader("X-Skin-Hash"));
        if (respHash.isEmpty()) {
            respHash = !expectedHash.isEmpty() ? expectedHash : computeSha256(data);
        }

        emit skinDownloaded(uuid, getCachedSkinPath(uuid), respModel, respHash);
    });
}

void XylarSkinClient::uploadSkin(const QString& uuid, const QString& pngFilePath, const QString& model)
{
    QString valErr;
    if (!validateSkinPng(pngFilePath, &valErr)) {
        emit requestFailed("uploadSkin", valErr, 400);
        return;
    }

    QFile file(pngFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit requestFailed("uploadSkin", tr("Cannot read skin file: %1").arg(file.errorString()), 400);
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    uploadSkinData(uuid, fileData, model);
}

void XylarSkinClient::uploadSkinData(const QString& uuid, const QByteArray& pngData, const QString& model)
{
    if (m_backendUrl.isEmpty()) {
        emit requestFailed("uploadSkin", tr("Backend URL is not configured."), 0);
        return;
    }

    QString valErr;
    if (!validateSkinPngData(pngData, &valErr)) {
        emit requestFailed("uploadSkin", valErr, 400);
        return;
    }

    if (playerKey(uuid).isEmpty()) {
        requirePlayerKey(uuid, "uploadSkin", [this, uuid, pngData, model] { uploadSkinData(uuid, pngData, model); });
        return;
    }
    QString skinModel = model.toLower() == "slim" ? "slim" : "classic";
    QJsonObject body;
    body["uuid"] = uuid;
    body["model"] = skinModel;
    body["skin_data"] = QString::fromLatin1(pngData.toBase64());

    QNetworkRequest req = makeRequest("/api/skin", true, uuid);
    QNetworkReply* reply = m_nam->post(req, QJsonDocument(body).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid, pngData, skinModel]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            QString err = parseErrorMessage(reply, data);
            emit requestFailed("uploadSkin", err, statusCode);
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject obj = doc.object();
        QString skinHash = obj["skin_hash"].toString();
        if (skinHash.isEmpty()) {
            skinHash = computeSha256(pngData);
        }

        // Cache the uploaded skin locally
        saveCachedSkin(uuid, pngData);

        emit skinUploaded(uuid, skinHash, skinModel);
    });
}

void XylarSkinClient::deleteSkin(const QString& uuid)
{
    if (playerKey(uuid).isEmpty()) {
        requirePlayerKey(uuid, "deleteSkin", [this, uuid] { deleteSkin(uuid); });
        return;
    }
    if (m_backendUrl.isEmpty()) {
        emit requestFailed("deleteSkin", tr("Backend URL is not configured."), 0);
        return;
    }

    QNetworkRequest req = makeRequest("/api/skin/" + uuid, false, uuid);
    QNetworkReply* reply = m_nam->deleteResource(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid]() {
        reply->deleteLater();
        QByteArray data = reply->readAll();
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            QString err = parseErrorMessage(reply, data);
            emit requestFailed("deleteSkin", err, statusCode);
            return;
        }

        // Remove local cache
        deleteCachedSkin(uuid);

        emit skinDeleted(uuid);
    });
}

// ─── Capes ────────────────────────────────────────────────────────────────────

void XylarSkinClient::fetchCapes()
{
    if (m_capeBackendUrl.isEmpty()) {
        emit requestFailed("fetchCapes", tr("Cape backend URL is not configured."), 0);
        return;
    }
    QNetworkReply* reply = m_nam->get(makeCapeRequest("/api/capes", false));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            emit requestFailed("fetchCapes", parseErrorMessage(reply, data), statusCode);
            return;
        }
        emit capesListed(QJsonDocument::fromJson(data).object().value("capes").toArray());
    });
}

void XylarSkinClient::fetchCapeMetadata(const QString& uuid)
{
    if (m_capeBackendUrl.isEmpty()) {
        emit requestFailed("fetchCapeMetadata", tr("Cape backend URL is not configured."), 0);
        return;
    }
    QNetworkReply* reply = m_nam->get(makeCapeRequest("/api/cape/" + uuid + "/metadata", false));
    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            emit requestFailed("fetchCapeMetadata", parseErrorMessage(reply, data), statusCode);
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        emit capeMetadataReceived(uuid, obj.value("has_cape").toBool(), obj.value("cape_hash").toString(), obj.value("cape_id").toString());
    });
}

QNetworkRequest XylarSkinClient::makeCapeRequest(const QString& endpoint, bool json, const QString& playerUuid) const
{
    // Same headers (the player's key included), sent to the cape backend.
    QNetworkRequest req = makeRequest(endpoint, json, playerUuid);
    req.setUrl(QUrl(m_capeBackendUrl + endpoint));
    req.setRawHeader("Authorization", QByteArray());  // the skins backend's token stays there
    return req;
}

QString XylarSkinClient::cachedCapePath(const QString& hash) const
{
    return QDir(m_cacheDir).filePath("capes/" + hash + ".png");
}

QByteArray XylarSkinClient::loadCachedCape(const QString& hash) const
{
    QFile f(cachedCapePath(hash));
    if (hash.isEmpty() || !f.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return f.readAll();
}

void XylarSkinClient::downloadCape(const QString& hash)
{
    static const QRegularExpression hashFormat("^[0-9a-f]{64}$");
    if (m_capeBackendUrl.isEmpty() || !hashFormat.match(hash).hasMatch()) {
        return;
    }
    const QByteArray cached = loadCachedCape(hash);
    if (!cached.isEmpty()) {
        emit capeDownloaded(hash, cached);
        return;
    }
    QNetworkReply* reply = m_nam->get(makeCapeRequest("/api/cape-texture/" + hash, false));
    connect(reply, &QNetworkReply::finished, this, [this, reply, hash]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            emit requestFailed("downloadCape", parseErrorMessage(reply, data), statusCode);
            return;
        }
        // Named by its hash: anything else is a broken download.
        if (computeSha256(data) != hash) {
            emit requestFailed("downloadCape", tr("Downloaded cape is damaged."), statusCode);
            return;
        }
        QDir().mkpath(QDir(m_cacheDir).filePath("capes"));
        QSaveFile out(cachedCapePath(hash));
        if (out.open(QIODevice::WriteOnly)) {
            out.write(data);
            out.commit();
        }
        emit capeDownloaded(hash, data);
    });
}

void XylarSkinClient::setCape(const QString& uuid, const QString& capeId)
{
    QJsonObject body;
    body["uuid"] = uuid;
    body["id"] = capeId;
    sendCapeChange("setCape", uuid, body);
}

void XylarSkinClient::uploadCapeData(const QString& uuid, const QByteArray& pngData)
{
    QString valErr;
    if (!validateCapePngData(pngData, &valErr)) {
        emit requestFailed("uploadCape", valErr, 400);
        return;
    }
    QJsonObject body;
    body["uuid"] = uuid;
    body["cape_data"] = QString::fromLatin1(pngData.toBase64());
    sendCapeChange("uploadCape", uuid, body);
}

void XylarSkinClient::sendCapeChange(const QString& action, const QString& uuid, const QJsonObject& body)
{
    if (playerKey(uuid).isEmpty()) {
        requirePlayerKey(uuid, action, [this, action, uuid, body] { sendCapeChange(action, uuid, body); });
        return;
    }
    if (m_capeBackendUrl.isEmpty()) {
        emit requestFailed(action, tr("Cape backend URL is not configured."), 0);
        return;
    }
    QNetworkReply* reply = m_nam->post(makeCapeRequest("/api/cape", true, uuid), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid, action]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            emit requestFailed(action, parseErrorMessage(reply, data), statusCode);
            return;
        }
        const QJsonObject obj = QJsonDocument::fromJson(data).object();
        emit capeChanged(uuid, obj.value("cape_hash").toString(), obj.value("cape_id").toString());
    });
}

void XylarSkinClient::removeCape(const QString& uuid)
{
    if (playerKey(uuid).isEmpty()) {
        requirePlayerKey(uuid, "removeCape", [this, uuid] { removeCape(uuid); });
        return;
    }
    if (m_capeBackendUrl.isEmpty()) {
        emit requestFailed("removeCape", tr("Cape backend URL is not configured."), 0);
        return;
    }
    QNetworkReply* reply = m_nam->deleteResource(makeCapeRequest("/api/cape/" + uuid, false, uuid));
    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            emit requestFailed("removeCape", parseErrorMessage(reply, data), statusCode);
            return;
        }
        emit capeChanged(uuid, QString(), QString());
    });
}

// ─── Ownership ────────────────────────────────────────────────────────────────

QString XylarSkinClient::keysFile() const
{
    return QDir(m_cacheDir).filePath(".player-keys.json");
}

QString XylarSkinClient::playerKey(const QString& uuid) const
{
    QFile f(keysFile());
    if (!f.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const QJsonObject keys = QJsonDocument::fromJson(f.readAll()).object();
    return AccountTokenCipher::decrypt(keys.value(uuid.toLower()).toString());
}

bool XylarSkinClient::setPlayerKey(const QString& uuid, const QString& key)
{
    QJsonObject keys;
    {
        QFile f(keysFile());
        if (f.open(QIODevice::ReadOnly)) {
            keys = QJsonDocument::fromJson(f.readAll()).object();
        }
    }
    keys[uuid.toLower()] = AccountTokenCipher::encrypt(key);
    QDir().mkpath(m_cacheDir);
    QSaveFile out(keysFile());
    if (out.open(QIODevice::WriteOnly)) {
        const QByteArray data = QJsonDocument(keys).toJson(QJsonDocument::Indented);
        if (out.write(data) != data.size() || !out.commit())
            return false;
        QFile::setPermissions(keysFile(), QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        return true;
    }
    return false;
}

void XylarSkinClient::requirePlayerKey(const QString& uuid, const QString& action, std::function<void()> operation)
{
    if (!playerKey(uuid).isEmpty()) {
        operation();
        return;
    }
    emit requestFailed(action,
                       tr("This installation cannot change this profile yet. Use Skin access to import access exported from the device "
                          "that owns the profile, or sign in with its verified account."),
                       403);
}

QByteArray XylarSkinClient::exportPlayerAccess(const QString& uuid) const
{
    const QString key = playerKey(uuid);
    if (key.isEmpty() || m_backendUrl.isEmpty())
        return {};
    return QJsonDocument(QJsonObject{ { "version", 1 }, { "backend", m_backendUrl }, { "uuid", uuid.toLower() }, { "key", key } })
        .toJson(QJsonDocument::Indented);
}

bool XylarSkinClient::importPlayerAccess(const QString& uuid, const QByteArray& data, QString* outError)
{
    auto fail = [outError](const QString& message) {
        if (outError)
            *outError = message;
        return false;
    };
    if (data.size() > 16384)
        return fail(tr("This is not a valid skin access file."));
    const QJsonObject object = QJsonDocument::fromJson(data).object();
    if (object.value("version").toInt() != 1 || object.value("backend").toString() != m_backendUrl || m_backendUrl.isEmpty())
        return fail(tr("This skin access file belongs to another backend or has an unsupported format."));
    if (uuid.isEmpty() || object.value("uuid").toString().compare(uuid, Qt::CaseInsensitive) != 0)
        return fail(tr("This skin access file belongs to another player. Select the matching account first."));
    const QString key = object.value("key").toString();
    static const QRegularExpression keyFormat(QStringLiteral("^[A-Za-z0-9_-]{32,256}$"));
    if (!keyFormat.match(key).hasMatch())
        return fail(tr("The skin access file does not contain a valid player key."));
    if (!setPlayerKey(uuid, key))
        return fail(tr("Could not save skin access on this computer."));
    return true;
}

void XylarSkinClient::startSiteLink(const QString& uuid)
{
    if (m_backendUrl.isEmpty()) {
        emit requestFailed("siteLink", tr("Backend URL is not configured."), 0);
        return;
    }
    if (!playerKey(uuid).isEmpty()) {
        requestLinkCode(uuid);
        return;
    }
    requirePlayerKey(uuid, "siteLink", [this, uuid] { requestLinkCode(uuid); });
}

void XylarSkinClient::requestLinkCode(const QString& uuid)
{
    QJsonObject body;
    body["uuid"] = uuid;
    QNetworkReply* reply = m_nam->post(makeRequest("/api/link/start", true, uuid), QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, uuid]() {
        reply->deleteLater();
        const QByteArray data = reply->readAll();
        const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || statusCode >= 400) {
            emit requestFailed("siteLink", parseErrorMessage(reply, data), statusCode);
            return;
        }
        emit siteLinkReady(uuid, QJsonDocument::fromJson(data).object().value("code").toString());
    });
}
