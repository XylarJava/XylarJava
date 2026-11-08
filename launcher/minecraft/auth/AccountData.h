#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <QDateTime>
#include <QMap>
#include <QVariantMap>

enum class Validity { None, Assumed, Certain };

struct Token {
    QDateTime issueInstant;
    QDateTime notAfter;
    QString token;
    QString refresh_token;
    QVariantMap extra;

    Validity validity = Validity::None;
    bool persistent = true;
};

struct Skin {
    QString id;
    QString url;
    QString variant;

    QByteArray data;
};

struct Cape {
    QString id;
    QString url;
    QString alias;

    QByteArray data;
};

struct MinecraftEntitlement {
    bool ownsMinecraft = false;
    bool canPlayMinecraft = false;
    Validity validity = Validity::None;
};

struct MinecraftProfile {
    QString id;
    QString name;
    Skin skin;
    QString currentCape;
    QMap<QString, Cape> capes;
    Validity validity = Validity::None;
};

enum class AccountType { MSA, Offline, Ely };

enum class AccountState { Unchecked, Offline, Working, Online, Disabled, Errored, Expired, Gone };

struct AccountData {
    QJsonObject saveState() const;
    bool resumeStateFromV3(QJsonObject data);

    //! Yggdrasil access token, as passed to the game.
    QString accessToken() const;

    QString profileId() const;
    QString profileName() const;

    QString lastError() const;

    AccountType type = AccountType::MSA;

    QString msaClientID;
    Token msaToken;
    Token userToken;
    Token mojangservicesToken;

    Token yggdrasilToken;
    MinecraftProfile minecraftProfile;
    MinecraftEntitlement minecraftEntitlement;
    Validity validity_ = Validity::None;

    QString elyLoginName;
    QString elyPassword;  // transient: never serialized

    // runtime only information (not saved with the account)
    QString internalId;
    QString errorString;
    AccountState accountState = AccountState::Unchecked;
};
