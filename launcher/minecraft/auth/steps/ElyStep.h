#pragma once
#include <QNetworkReply>
#include <QPointer>
#include "minecraft/auth/AuthStep.h"

class QNetworkAccessManager;
class ElyStep : public AuthStep {
    Q_OBJECT
   public:
    explicit ElyStep(AccountData* data, bool refresh, QNetworkAccessManager* network = nullptr);
    QString describe() override;
    void perform() override;
    void abort() override;

   private:
    void profile();
    void skin(const QJsonObject& profile);
    bool m_refresh;
    QNetworkAccessManager* m_network;
    QPointer<QNetworkReply> m_reply;
};
