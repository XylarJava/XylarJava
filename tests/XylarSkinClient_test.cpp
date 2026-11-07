#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "minecraft/skins/XylarSkinClient.h"

namespace {
const QString PLAYER_UUID = QStringLiteral("01234567-89ab-3cde-8fab-0123456789ab");
const QString PLAYER_KEY = QString(43, QLatin1Char('k'));
const QString SKIN_BACKEND = QStringLiteral("https://skins.example.test");

class Reply : public QNetworkReply {
   public:
    Reply(const QNetworkRequest& request, int status, const QByteArray& data, QObject* parent) : QNetworkReply(parent), m_data(data)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status >= 400)
            setError(QNetworkReply::ContentAccessDenied, QStringLiteral("Request rejected"));
        open(QIODevice::ReadOnly);
        QTimer::singleShot(10, this, [this] {
            setFinished(true);
            emit readyRead();
            emit finished();
        });
    }
    void abort() override {}
    qint64 bytesAvailable() const override { return m_data.size() - m_pos + QNetworkReply::bytesAvailable(); }

   protected:
    qint64 readData(char* output, qint64 maximum) override
    {
        const qint64 count = qMin(maximum, qint64(m_data.size()) - m_pos);
        if (count <= 0)
            return -1;
        memcpy(output, m_data.constData() + m_pos, size_t(count));
        m_pos += count;
        return count;
    }

   private:
    QByteArray m_data;
    qint64 m_pos = 0;
};

class Network : public QNetworkAccessManager {
   public:
    struct Answer {
        int status;
        QByteArray data;
    };
    QQueue<Answer> answers;
    QList<QNetworkRequest> requests;

   protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice*) override
    {
        requests.append(request);
        const Answer answer = answers.isEmpty() ? Answer{ 500, R"({"error":"Unexpected request"})" } : answers.dequeue();
        return new Reply(request, answer.status, answer.data, this);
    }
};

QByteArray png()
{
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);
    QByteArray data;
    QBuffer output(&data);
    output.open(QIODevice::WriteOnly);
    image.save(&output, "PNG");
    return data;
}

QByteArray access()
{
    return QJsonDocument(QJsonObject{ { "version", 1 }, { "backend", SKIN_BACKEND }, { "uuid", PLAYER_UUID }, { "key", PLAYER_KEY } })
        .toJson();
}
}  // namespace

class XylarSkinClientTest : public QObject {
    Q_OBJECT
   private slots:
    void registrationWaitsForOwnershipBeforeAutomaticUpload()
    {
        QTemporaryDir cache;
        Network network;
        network.answers.enqueue({ 201, QJsonDocument(QJsonObject{ { "player", QJsonObject{ { "uuid", PLAYER_UUID } } } }).toJson() });
        network.answers.enqueue({ 201, QJsonDocument(QJsonObject{ { "key", PLAYER_KEY } }).toJson() });
        network.answers.enqueue({ 200, R"({"skin_hash":"saved"})" });
        XylarSkinClient client(nullptr, &network);
        client.setCacheDir(cache.path());
        client.setBackendUrl(SKIN_BACKEND);
        bool ready = false;
        connect(&client, &XylarSkinClient::playerRegistered, &client, [&](const QString& uuid) {
            ready = client.playerKey(uuid) == PLAYER_KEY;
            client.uploadSkinData(uuid, png());
        });
        QSignalSpy uploaded(&client, &XylarSkinClient::skinUploaded);
        client.registerPlayer(QStringLiteral("Player"));
        QTRY_COMPARE(uploaded.count(), 1);
        QVERIFY(ready);
        QCOMPARE(network.requests.size(), 3);
        QCOMPARE(network.requests.last().rawHeader("X-Player-Key"), PLAYER_KEY.toLatin1());
    }

    void concurrentMutationsShareOneClaim()
    {
        QTemporaryDir cache;
        Network network;
        network.answers.enqueue({ 201, QJsonDocument(QJsonObject{ { "key", PLAYER_KEY } }).toJson() });
        XylarSkinClient client(nullptr, &network);
        client.setCacheDir(cache.path());
        client.setBackendUrl(SKIN_BACKEND);
        int completed = 0;
        client.claimPlayer(PLAYER_UUID, [&](bool ok) { completed += ok; });
        client.claimPlayer(PLAYER_UUID, [&](bool ok) { completed += ok; });
        QTRY_COMPARE(completed, 2);
        QCOMPARE(network.requests.size(), 1);
    }

    void claimedElsewhereDoesNotSendUnauthenticatedUpload()
    {
        QTemporaryDir cache;
        Network network;
        network.answers.enqueue({ 409, R"({"error":"Already claimed"})" });
        XylarSkinClient client(nullptr, &network);
        client.setCacheDir(cache.path());
        client.setBackendUrl(SKIN_BACKEND);
        QSignalSpy failed(&client, &XylarSkinClient::requestFailed);
        client.uploadSkinData(PLAYER_UUID, png());
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(network.requests.size(), 1);
        QCOMPARE(failed.first().at(2).toInt(), 403);
        QVERIFY(failed.first().at(1).toString().contains(QStringLiteral("Skin access")));
    }

    void backendErrorsKeepTheirActualStatus()
    {
        QTemporaryDir cache;
        Network network;
        network.answers.enqueue({ 503, R"({"error":"Service unavailable"})" });
        XylarSkinClient client(nullptr, &network);
        client.setCacheDir(cache.path());
        client.setBackendUrl(SKIN_BACKEND);
        QSignalSpy failed(&client, &XylarSkinClient::requestFailed);
        client.uploadSkinData(PLAYER_UUID, png());
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed.first().at(2).toInt(), 503);
    }

    void portableAccessIsScopedToThePlayerAndBackend()
    {
        QTemporaryDir linuxCache;
        QTemporaryDir windowsCache;
        XylarSkinClient linuxClient;
        linuxClient.setCacheDir(linuxCache.path());
        linuxClient.setBackendUrl(SKIN_BACKEND);
        QVERIFY(linuxClient.importPlayerAccess(PLAYER_UUID, access()));
        XylarSkinClient windowsClient;
        windowsClient.setCacheDir(windowsCache.path());
        windowsClient.setBackendUrl(SKIN_BACKEND);
        QVERIFY(windowsClient.importPlayerAccess(PLAYER_UUID, linuxClient.exportPlayerAccess(PLAYER_UUID)));
        QCOMPARE(windowsClient.playerKey(PLAYER_UUID), PLAYER_KEY);
        QVERIFY(!windowsClient.importPlayerAccess(QStringLiteral("another-player"), access()));
        windowsClient.setBackendUrl(QStringLiteral("https://another.example.test"));
        QVERIFY(!windowsClient.importPlayerAccess(PLAYER_UUID, access()));
        QCOMPARE(windowsClient.playerKey(PLAYER_UUID), PLAYER_KEY);
    }

    void freshInstallationDownloadsSkinWithoutAnOwnershipKey()
    {
        QTemporaryDir cache;
        Network network;
        const QByteArray texture = png();
        const QString hash = XylarSkinClient::computeSha256(texture);
        network.answers.enqueue(
            { 200, QJsonDocument(QJsonObject{ { "has_skin", true }, { "skin_hash", hash }, { "model", "classic" } }).toJson() });
        network.answers.enqueue({ 200, texture });
        XylarSkinClient client(nullptr, &network);
        client.setCacheDir(cache.path());
        client.setBackendUrl(SKIN_BACKEND);
        connect(&client, &XylarSkinClient::skinMetadataReceived, &client,
                [&](const QString& uuid, bool hasSkin, const QString& skinHash, const QString& model) {
                    if (hasSkin)
                        client.downloadSkin(uuid, skinHash, model);
                });
        QSignalSpy downloaded(&client, &XylarSkinClient::skinDownloaded);
        client.fetchSkinMetadata(PLAYER_UUID);
        QTRY_COMPARE(downloaded.count(), 1);
        QCOMPARE(client.loadCachedSkin(PLAYER_UUID), texture);
        QVERIFY(client.playerKey(PLAYER_UUID).isEmpty());
        QCOMPARE(network.requests.size(), 2);
    }
};

QTEST_GUILESS_MAIN(XylarSkinClientTest)
#include "XylarSkinClient_test.moc"
