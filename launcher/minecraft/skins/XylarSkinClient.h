#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <functional>

class XylarSkinClient : public QObject {
    Q_OBJECT

   public:
    explicit XylarSkinClient(QObject* parent = nullptr, QNetworkAccessManager* network = nullptr);
    ~XylarSkinClient() override = default;

    void setBackendUrl(const QString& url);
    QString backendUrl() const;
    /// Capes have their own backend; players and their keys are the skins backend's.
    void setCapeBackendUrl(const QString& url);
    QString capeBackendUrl() const;

    void setAuthToken(const QString& token);
    QString authToken() const;

    void setCacheDir(const QString& dir);
    QString cacheDir() const;

    // Cache helpers
    QString getCachedSkinPath(const QString& uuid) const;
    bool hasCachedSkin(const QString& uuid) const;
    QByteArray loadCachedSkin(const QString& uuid) const;
    bool saveCachedSkin(const QString& uuid, const QByteArray& data);
    bool deleteCachedSkin(const QString& uuid);

    // Static utilities
    static QString defaultCacheDir();
    static QString computeOfflineUUID(const QString& username);
    static bool validateSkinPng(const QString& filePath, QString* outError = nullptr);
    static bool validateSkinPngData(const QByteArray& data, QString* outError = nullptr);
    static QString computeSha256(const QByteArray& data);
    /// A cape is a PNG twice as wide as it is tall, from 64x32 to 2048x1024.
    static bool validateCapePngData(const QByteArray& data, QString* outError = nullptr);

    // API Operations
    void registerPlayer(const QString& username,
                        const QString& profileId = QString(),
                        const QString& identityToken = QString(),
                        const QString& provider = QString(),
                        const QString& ownedUuid = QString());
    void fetchPlayer(const QString& uuid);
    void fetchSkinMetadata(const QString& uuid);
    void downloadSkin(const QString& uuid, const QString& expectedHash = QString(), const QString& model = "classic");
    void uploadSkin(const QString& uuid, const QString& pngFilePath, const QString& model = "classic");
    void uploadSkinData(const QString& uuid, const QByteArray& pngData, const QString& model = "classic");
    void deleteSkin(const QString& uuid);
    void checkHealth();

    // Capes: one of the built-in XylarJava capes, or a PNG the player uploads.
    void fetchCapes();
    void fetchCapeMetadata(const QString& uuid);
    /// Cape PNGs are named by their SHA-256, so they are kept on disk for good.
    void downloadCape(const QString& hash);
    QByteArray loadCachedCape(const QString& hash) const;
    void setCape(const QString& uuid, const QString& capeId);
    void uploadCapeData(const QString& uuid, const QByteArray& pngData);
    void removeCape(const QString& uuid);

    // Ownership: a claimed player's skin changes only with its key, which is kept here.
    QString playerKey(const QString& uuid) const;
    QByteArray exportPlayerAccess(const QString& uuid) const;
    bool importPlayerAccess(const QString& uuid, const QByteArray& data, QString* outError = nullptr);
    /// Asks for a short code to link an owned profile to a Xylar website account.
    void startSiteLink(const QString& uuid);

   signals:
    void playerRegistered(const QString& uuid, const QString& username, const QString& skinHash, const QString& skinModel);
    void playerFetched(const QString& uuid, const QString& username, const QString& skinHash, const QString& skinModel);
    void skinMetadataReceived(const QString& uuid, bool hasSkin, const QString& skinHash, const QString& model);
    void skinDownloaded(const QString& uuid, const QString& localPath, const QString& model, const QString& skinHash);
    void skinUploaded(const QString& uuid, const QString& skinHash, const QString& model);
    void skinDeleted(const QString& uuid);
    void healthChecked(bool ok, const QString& service);
    void siteLinkReady(const QString& uuid, const QString& code);
    /// Each entry: { id, name, hash, url }.
    void capesListed(const QJsonArray& capes);
    void capeMetadataReceived(const QString& uuid, bool hasCape, const QString& capeHash, const QString& capeId);
    void capeDownloaded(const QString& hash, const QByteArray& pngData);
    /// After setCape, uploadCapeData or removeCape; an empty hash means no cape.
    void capeChanged(const QString& uuid, const QString& capeHash, const QString& capeId);
    void requestFailed(const QString& action, const QString& errorMessage, int httpStatusCode);

   private:
    QNetworkRequest makeRequest(const QString& endpoint, bool json = true, const QString& playerUuid = QString()) const;
    QString keysFile() const;
    bool setPlayerKey(const QString& uuid, const QString& key);
    void requirePlayerKey(const QString& uuid, const QString& action, std::function<void()> operation);
    void requestLinkCode(const QString& uuid);
    QString parseErrorMessage(QNetworkReply* reply, const QByteArray& responseBody) const;
    QString cachedCapePath(const QString& hash) const;
    QNetworkRequest makeCapeRequest(const QString& endpoint, bool json = true, const QString& playerUuid = QString()) const;
    void sendCapeChange(const QString& action, const QString& uuid, const QJsonObject& body);

   private:
    QNetworkAccessManager* m_nam;
    QString m_backendUrl;
    QString m_capeBackendUrl;
    QString m_authToken;
    QString m_cacheDir;
};
