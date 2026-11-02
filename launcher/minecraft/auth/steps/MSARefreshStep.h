#pragma once
#include <QObject>

#include "minecraft/auth/AuthStep.h"
#include "net/NetJob.h"
#include "net/Upload.h"

/**
 * Silent sign-in with the saved Microsoft refresh token, the same request XylarJava for Xbox makes:
 * a refresh_token grant on login.microsoftonline.com/consumers with the "XboxLive.signin
 * offline_access" scopes. Microsoft may hand back a new refresh token, which replaces the old one.
 */
class MSARefreshStep : public AuthStep {
    Q_OBJECT
   public:
    explicit MSARefreshStep(AccountData* data);
    virtual ~MSARefreshStep() noexcept = default;

    void perform() override;
    QString describe() override;

   public slots:
    void abort() override;

   private:
    void onRequestDone(QByteArray* response);

    QString m_clientId;
    Net::Upload::Ptr m_request;
    NetJob::Ptr m_task;
};
