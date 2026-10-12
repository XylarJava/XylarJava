#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "minecraft/auth/steps/MSAStep.h"

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

   signals:
    void authorizeWithBrowser(const QUrl& url);

   private:
    void useBrowserFlow(const QString& reason);

    bool m_silent;
    QString m_clientId;
    shared_qobject_ptr<MSAStep> m_fallback;
};
