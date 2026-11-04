#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QObject;

/**
 * Calls to the XylarJava services that sign requests as the Social player (profiles):
 * JSON in and out, with the player's headers from SocialClient::current().
 */
namespace XylarService {
using Done = std::function<void(int status, const QJsonObject& body)>;

/// @p url without the trailing slashes.
QString baseUrl(const QString& url);

/// Sends @p payload (JSON unless @p contentType says otherwise) and calls @p done on @p context's
/// thread with the HTTP status (0 when the service can't be reached) and the JSON answer. Returns
/// null, and calls nothing, when there's no Social player with a key yet.
QNetworkReply* call(QNetworkAccessManager* network, const QString& url, const QByteArray& method, const QByteArray& payload,
                    QObject* context, Done done, const QByteArray& contentType = "application/json", int timeoutMs = 20000);

/// What to tell the player about a failed call to @p service (e.g. "XylarJava Profiles").
QString messageFor(const QString& service, int status, const QJsonObject& body);
}  // namespace XylarService
