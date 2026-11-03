#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QStandardPaths>
#include <QString>

class XylarSkinClient : public QObject {
    Q_OBJECT

   public:
    explicit XylarSkinClient(QObject* parent = nullptr);
    ~XylarSkinClient() override = default;

    void setBackendUrl(const QString& url);
    QString backendUrl() const;

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

    // API Operations
    void registerPlayer(const QString& username, const QString& msaUuid = QString());
    void fetchPlayer(const QString& uuid);
    void fetchSkinMetadata(const QString& uuid);
    void downloadSkin(const QString& uuid, const QString& expectedHash = QString(), const QString& model = "classic");
    void uploadSkin(const QString& uuid, const QString& pngFilePath, const QString& model = "classic");
    void uploadSkinData(const QString& uuid, const QByteArray& pngData, const QString& model = "classic");
    void deleteSkin(const QString& uuid);
    void checkHealth();

    // Ownership: a claimed player's skin changes only with its key, which is kept here.
    QString playerKey(const QString& uuid) const;
    void claimPlayer(const QString& uuid);
    /// Asks for a short code to link this player to a Xylar website account (claims it first if needed).
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
    void requestFailed(const QString& action, const QString& errorMessage, int httpStatusCode);

   private:
    QNetworkRequest makeRequest(const QString& endpoint, bool json = true, const QString& playerUuid = QString()) const;
    QString keysFile() const;
    void setPlayerKey(const QString& uuid, const QString& key);
    void requestLinkCode(const QString& uuid);
    QString parseErrorMessage(QNetworkReply* reply, const QByteArray& responseBody) const;

   private:
    QNetworkAccessManager* m_nam;
    QString m_backendUrl;
    QString m_authToken;
    QString m_cacheDir;
};
