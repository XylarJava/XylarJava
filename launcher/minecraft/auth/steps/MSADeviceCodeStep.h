#pragma once
#include <QDeadlineTimer>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

#include "minecraft/auth/AuthStep.h"

class QNetworkReply;

/**
 * Microsoft sign-in with a device code (microsoft.com/link), the same as XylarJava for Xbox:
 * ask for a code, then ask for the token every `interval` seconds until it's approved or the
 * code expires. "authorization_pending" keeps waiting, "slow_down" waits 5 seconds longer,
 * anything else stops with Microsoft's message. The code never changes while it's on screen.
 */
class MSADeviceCodeStep : public AuthStep {
    Q_OBJECT
   public:
    explicit MSADeviceCodeStep(AccountData* data);
    virtual ~MSADeviceCodeStep() noexcept = default;

    void perform() override;

    QString describe() override;

   public slots:
    void abort() override;

   signals:
    void authorizeWithBrowser(QString url, QString code, int expiresIn);

   private:
    /// POST a form to login.microsoftonline.com; done gets the HTTP status (0 without an answer) and the body.
    void post(const QString& url, const QByteArray& form, std::function<void(int status, const QByteArray& body)> done);
    void schedulePoll();
    void poll();
    void fail(const QString& message);

    QString m_clientId;
    QString m_deviceCode;
    bool m_isAborted = false;
    bool m_finished = false;
    int m_interval = 5;
    QDeadlineTimer m_expiresAt;

    QTimer m_pollTimer;
    QPointer<QNetworkReply> m_reply;
};
