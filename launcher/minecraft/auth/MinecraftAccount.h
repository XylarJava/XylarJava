#pragma once

#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QPair>
#include <QPixmap>
#include <QString>

#include "AccountData.h"
#include "AuthSession.h"
#include "QObjectPtr.h"
#include "Usable.h"
#include "minecraft/auth/AuthFlow.h"

class Task;
class MinecraftAccount;

using MinecraftAccountPtr = shared_qobject_ptr<MinecraftAccount>;
Q_DECLARE_METATYPE(MinecraftAccountPtr)

/**
 * A profile within someone's Mojang account.
 *
 * Currently, the profile system has not been implemented by Mojang yet,
 * but we might as well add some things for it in PolyMC right now so
 * we don't have to rip the code to pieces to add it later.
 */
struct AccountProfile {
    QString id;
    QString name;
    bool legacy;
};

/**
 * Object that stores information about a certain Mojang account.
 *
 * Said information may include things such as that account's username, client token, and access
 * token if the user chose to stay logged in.
 */
class MinecraftAccount : public QObject, public Usable {
    Q_OBJECT
   public: /* construction */
    //! Do not copy accounts. ever.
    explicit MinecraftAccount(const MinecraftAccount& other, QObject* parent) = delete;

    //! Default constructor
    explicit MinecraftAccount(QObject* parent = 0);

    static MinecraftAccountPtr createBlankMSA();

    static MinecraftAccountPtr createOffline(const QString& username);

    static MinecraftAccountPtr loadFromJsonV3(const QJsonObject& json);

    static QUuid uuidFromUsername(QString username);

    //! Saves a MinecraftAccount to a JSON object and returns it.
    QJsonObject saveToJson() const;

   public: /* manipulation */
    shared_qobject_ptr<AuthFlow> login(bool useDeviceCode = false);

    shared_qobject_ptr<AuthFlow> refresh();

    shared_qobject_ptr<AuthFlow> currentTask();

   public: /* queries */
    QString internalId() const { return data.internalId; }

    QString accessToken() const { return data.accessToken(); }

    QString profileId() const { return data.profileId(); }

    QString profileName() const { return data.profileName(); }

    QString displayName() const;

    bool isActive() const;

    AccountType accountType() const noexcept { return data.type; }

    bool ownsMinecraft() const { return true; }

    bool hasProfile() const { return data.profileId().size() != 0; }

    QString typeString() const
    {
        switch (data.type) {
            case AccountType::MSA: {
                return "msa";
            } break;
            case AccountType::Offline: {
                return "offline";
            } break;
            default: {
                return "unknown";
            }
        }
    }

    QPixmap getFace(int width = 64, int height = 64) const;

    //! Returns the current state of the account
    AccountState accountState() const;

    AccountData* accountData() { return &data; }

    bool shouldRefresh() const;

    void fillSession(AuthSessionPtr session);

    QString lastError() const { return data.lastError(); }

   signals:
    /**
     * This signal is emitted when the account changes
     */
    void changed();

    void activityChanged(bool active);

    // TODO: better signalling for the various possible state changes - especially errors

   protected: /* variables */
    AccountData data;

    // current task we are executing here
    shared_qobject_ptr<AuthFlow> m_currentTask;

   protected: /* methods */
    void incrementUses() override;
    void decrementUses() override;

   private slots:
    void authSucceeded();
    void authFailed(QString reason);
};
