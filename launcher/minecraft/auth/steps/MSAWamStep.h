#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "minecraft/auth/steps/MSADeviceCodeStep.h"
#include "minecraft/auth/steps/MSARefreshStep.h"

// Microsoft sign-in through the Windows token broker, with the browser flow (MSAStep) as the
// fallback. Login shows the Windows account picker; Refresh gets a new token silently for
// accounts that were added through the broker and uses the saved refresh token otherwise.
class MSAWamStep : public AuthStep {
    Q_OBJECT
   public:
    explicit MSAWamStep(AccountData* data, bool silent = false);
    virtual ~MSAWamStep() noexcept = default;

    void perform() override;

    QString describe() override;

   public slots:
    void abort() override;

   signals:
    /// The sign-in continues with a device code (see MSADeviceCodeStep::authorizeWithBrowser).
    void authorizeWithDeviceCode(QString url, QString code, int expiresIn);

   private:
    /// Without the Windows token broker: the saved refresh token when signing in silently, a device
    /// code otherwise. That's the order XylarJava for Xbox uses too.
    void useFallback(const QString& reason);

    bool m_silent;
    QString m_clientId;
    AuthStep::Ptr m_fallback;
};
