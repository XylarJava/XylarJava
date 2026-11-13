#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QQueue>
#include <QSignalSpy>
#include <QTest>
#include <QTimer>
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountTokenCipher.h"
#include "minecraft/auth/AuthSession.h"
#include "minecraft/auth/steps/ElyStep.h"
namespace {
class Reply : public QNetworkReply {
   public:
    Reply(const QNetworkRequest& request, int status, QByteArray body, QObject* parent) : QNetworkReply(parent), m_body(body)
    {
        setRequest(request);
        setUrl(request.url());
        setAttribute(QNetworkRequest::HttpStatusCodeAttribute, status);
        if (status >= 400)
            setError(QNetworkReply::ContentAccessDenied, QStringLiteral("Rejected"));
        open(QIODevice::ReadOnly);
        QTimer::singleShot(0, this, [this] {
            setFinished(true);
            emit readyRead();
            emit finished();
        });
    }
    void abort() override {}

   protected:
    qint64 readData(char* out, qint64 max) override
    {
        const auto count = qMin(max, qint64(m_body.size()) - m_pos);
        if (count <= 0)
            return -1;
        memcpy(out, m_body.constData() + m_pos, size_t(count));
        m_pos += count;
        return count;
    }

   private:
    QByteArray m_body;
    qint64 m_pos = 0;
};
class Network : public QNetworkAccessManager {
   public:
    QQueue<QPair<int, QByteArray>> answers;
    QList<QNetworkRequest> requests;
    QList<QByteArray> bodies;

   protected:
    QNetworkReply* createRequest(Operation, const QNetworkRequest& request, QIODevice* data) override
    {
        requests << request;
        bodies << (data ? data->readAll() : QByteArray());
        const auto answer = answers.isEmpty() ? qMakePair(500, QByteArray()) : answers.dequeue();
        return new Reply(request, answer.first, answer.second, this);
    }
};
const QString id = QStringLiteral("12345678123412341234123456789abc");
QByteArray authenticated()
{
    return QJsonDocument(
               QJsonObject{ { "accessToken", "session-token" }, { "selectedProfile", QJsonObject{ { "id", id }, { "name", "ElyUser" } } } })
        .toJson();
}
QByteArray profile()
{
    const auto texture =
        QJsonDocument(
            QJsonObject{ { "textures", QJsonObject{ { "SKIN", QJsonObject{ { "url", "http://ely.by/storage/skins/test.png" },
                                                                           { "metadata", QJsonObject{ { "model", "slim" } } } } } } } })
            .toJson()
            .toBase64();
    return QJsonDocument(QJsonObject{ { "properties",
                                        QJsonArray{ QJsonObject{ { "name", "textures" }, { "value", QString::fromLatin1(texture) } } } } })
        .toJson();
}
}  // namespace
class AccountProvidersTest : public QObject {
    Q_OBJECT
   private slots:
    void microsoftRefreshCredentialsSurviveSaveWithoutPlaintextCopies()
    {
        AccountData original;
        original.type = AccountType::MSA;
        original.msaClientID = "application-id";
        original.msaToken.token = "private-access-token";
        original.msaToken.refresh_token = "long-lived-refresh-token";
        original.msaToken.extra = {{"access_token", original.msaToken.token}, {"refresh_token", original.msaToken.refresh_token}, {"wam_account_id", "broker-account"}};
        original.minecraftProfile.id = id;
        original.minecraftProfile.name = "MicrosoftUser";
        const auto saved = original.saveState();
        const auto serialized = QJsonDocument(saved).toJson();
        QVERIFY(!serialized.contains("private-access-token"));
        QVERIFY(!serialized.contains("long-lived-refresh-token"));
        AccountData restored;
        QVERIFY(restored.resumeStateFromV3(saved));
        QCOMPARE(restored.msaToken.refresh_token, original.msaToken.refresh_token);
        QCOMPARE(restored.msaToken.extra.value("wam_account_id").toString(), QStringLiteral("broker-account"));
        QVERIFY(!restored.msaToken.extra.contains("refresh_token"));
    }
    void offlineAliasCannotKeepMicrosoftIdentity()
    {
        AuthSession session;
        session.user_type = "msa";
        session.uuid = "old-premium-id";
        session.access_token = "premium-session";
        session.MakeOffline(QStringLiteral("OtherName"));
        QCOMPARE(session.user_type, QStringLiteral("offline"));
        QVERIFY(session.uuid != "old-premium-id");
        QCOMPARE(session.launchMode, LaunchMode::Offline);
        QCOMPARE(session.access_token, QStringLiteral("0"));
    }
    void elyLoginGetsVerifiedProfileAndProviderSkinWithoutSavingPassword()
    {
        AccountData data;
        data.type = AccountType::Ely;
        data.elyLoginName = "user@example.test";
        data.elyPassword = "password:123456";
        Network network;
        network.answers.enqueue({ 200, authenticated() });
        network.answers.enqueue({ 200, profile() });
        ElyStep step(&data, false, &network);
        QSignalSpy done(&step, &AuthStep::finished);
        step.perform();
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(done.first().first().value<AccountTaskState>(), AccountTaskState::STATE_WORKING);
        QCOMPARE(data.minecraftProfile.id, id);
        QCOMPARE(data.minecraftProfile.name, QStringLiteral("ElyUser"));
        QCOMPARE(data.minecraftProfile.skin.url, QStringLiteral("https://ely.by/storage/skins/test.png"));
        QCOMPARE(data.minecraftProfile.skin.variant, QStringLiteral("slim"));
        QVERIFY(data.elyPassword.isEmpty());
        const auto sent = QJsonDocument::fromJson(network.bodies.first()).object();
        QCOMPARE(sent["password"].toString(), QStringLiteral("password:123456"));
        const auto stored = QJsonDocument(data.saveState()).toJson();
        QVERIFY(!stored.contains("password"));
        QVERIFY(!stored.contains("session-token"));
        QVERIFY(!stored.contains("user@example"));
        AccountData restored;
        QVERIFY(restored.resumeStateFromV3(data.saveState()));
        QCOMPARE(restored.type, AccountType::Ely);
        QCOMPARE(restored.accessToken(), QStringLiteral("session-token"));
    }
    void elyRefreshUsesSessionAndClientTokenRatherThanPassword()
    {
        AccountData data;
        data.type = AccountType::Ely;
        data.minecraftProfile.id = id;
        data.yggdrasilToken.token = "old-session";
        data.yggdrasilToken.extra["clientToken"] = "device";
        Network network;
        network.answers.enqueue({ 200, authenticated() });
        network.answers.enqueue({ 200, profile() });
        ElyStep step(&data, true, &network);
        QSignalSpy done(&step, &AuthStep::finished);
        step.perform();
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(done.first().first().value<AccountTaskState>(), AccountTaskState::STATE_WORKING);
        QCOMPARE(network.requests.first().url().path(), QStringLiteral("/auth/refresh"));
        const auto sent = QJsonDocument::fromJson(network.bodies.first()).object();
        QCOMPARE(sent["accessToken"].toString(), QStringLiteral("old-session"));
        QCOMPARE(sent["clientToken"].toString(), QStringLiteral("device"));
        QVERIFY(!sent.contains("password"));
    }
    void elyTwoFactorErrorDoesNotAcceptAccount()
    {
        AccountData data;
        data.type = AccountType::Ely;
        data.elyPassword = "password";
        Network network;
        network.answers.enqueue({ 401, R"({"errorMessage":"Account protected with two factor auth."})" });
        ElyStep step(&data, false, &network);
        QSignalSpy done(&step, &AuthStep::finished);
        step.perform();
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(done.first().first().value<AccountTaskState>(), AccountTaskState::STATE_FAILED_HARD);
        QVERIFY(data.minecraftProfile.id.isEmpty());
        QVERIFY(data.elyPassword.isEmpty());
        QCOMPARE(network.requests.size(), 1);
    }
    void elyRefreshCannotSwitchIdentity()
    {
        AccountData data;
        data.type = AccountType::Ely;
        data.minecraftProfile.id = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
        Network network;
        network.answers.enqueue({ 200, authenticated() });
        ElyStep step(&data, true, &network);
        QSignalSpy done(&step, &AuthStep::finished);
        step.perform();
        QTRY_COMPARE(done.count(), 1);
        QCOMPARE(done.first().first().value<AccountTaskState>(), AccountTaskState::STATE_FAILED_SOFT);
        QCOMPARE(network.requests.size(), 1);
    }
};
QTEST_GUILESS_MAIN(AccountProvidersTest)
#include "AccountProviders_test.moc"
