#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

class QNetworkAccessManager;
class QNetworkReply;
class QTcpServer;
class QTcpSocket;

/**
 * The authentication server authlib-injector talks to, running inside the launcher on 127.0.0.1.
 *
 * Mojang refuses every request that comes from Cloudflare Workers, so the XylarSkins backend can't
 * pass a Microsoft account's server join, chat keys or name lookups on to Mojang. This relay sends
 * those straight to Mojang from this computer (like the game would without authlib-injector) and
 * everything else (the XylarSkins skins, offline accounts' joins) to the backend:
 *
 * - join: an owned Xylar session goes to the backend; Microsoft sessions go to Mojang.
 * - hasJoined (for servers hosted here): Mojang first, with the player's XylarSkins skin swapped in
 *   when they have one; otherwise the backend checks for an offline account that joined.
 * - /minecraftservices/... and /api/...: Mojang.
 * - the rest: the backend.
 *
 * Games and servers started by the launcher use it; it stops when the launcher quits.
 */
class YggdrasilRelay : public QObject {
    Q_OBJECT

   public:
    YggdrasilRelay(QNetworkAccessManager* network, QObject* parent = nullptr);

    /// The relay for @p backendUrl (started on first use), e.g. "http://127.0.0.1:41234"; empty if it can't listen.
    static QString urlFor(const QString& backendUrl);

   private:
    struct Request {
        QByteArray method;
        QByteArray path;  ///< without the query
        QByteArray query;  ///< with the leading '?', or empty
        QHash<QByteArray, QByteArray> headers;  ///< lowercase names
        QByteArray body;
    };
    struct Answer {
        int status = 502;
        QByteArray contentType;
        QByteArray body;
    };
    using Done = std::function<void(const Answer& answer)>;

    bool listen();
    void onConnection();
    void handle(QTcpSocket* socket, const Request& request);
    void forward(const QString& url, const Request& request, Done done);
    void join(const Request& request, Done done);
    void hasJoined(const Request& request, Done done);
    static void reply(QTcpSocket* socket, const Answer& answer);

    QNetworkAccessManager* m_network = nullptr;
    QTcpServer* m_server = nullptr;
    QString m_backendUrl;
};
