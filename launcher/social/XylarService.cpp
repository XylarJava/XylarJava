#include "XylarService.h"

#include <QCoreApplication>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QUrl>

#include "SocialClient.h"

namespace XylarService {

QString baseUrl(const QString& url)
{
    QString trimmed = url.trimmed();
    while (trimmed.endsWith(QLatin1Char('/')))
        trimmed.chop(1);
    return trimmed;
}

QNetworkReply* call(QNetworkAccessManager* network, const QString& url, const QByteArray& method, const QByteArray& payload,
                    QObject* context, Done done, const QByteArray& contentType, int timeoutMs)
{
    SocialClient* social = SocialClient::current();
    QNetworkRequest request{ QUrl(url) };
    if (!network || !social || !social->authorize(request))
        return nullptr;
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(contentType));
    request.setRawHeader("Accept", "application/json");
    if (timeoutMs > 0)
        request.setTransferTimeout(timeoutMs);
    QNetworkReply* reply = network->sendCustomRequest(request, method, payload);
    QObject::connect(reply, &QNetworkReply::finished, context, [reply, done] {
        reply->deleteLater();
        if (!done)
            return;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        done(status, QJsonDocument::fromJson(reply->readAll()).object());
    });
    // Without a context left to answer to, the reply still has to go.
    QObject::connect(context, &QObject::destroyed, reply, [reply = QPointer<QNetworkReply>(reply), context] {
        if (reply) {
            // Nothing may answer the context while it's being destroyed.
            QObject::disconnect(reply, nullptr, context, nullptr);
            reply->abort();
            reply->deleteLater();
        }
    });
    return reply;
}

QString messageFor(const QString& service, int status, const QJsonObject& body)
{
    const QString error = body.value("error").toString();
    if (!error.isEmpty())
        return error;
    if (status == 0)
        return QCoreApplication::translate("XylarService", "%1 can't be reached. Check your connection.").arg(service);
    return QCoreApplication::translate("XylarService", "%1 answered HTTP %2.").arg(service).arg(status);
}

}  // namespace XylarService
