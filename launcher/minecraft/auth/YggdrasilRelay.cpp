#include "YggdrasilRelay.h"

#include <QCoreApplication>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QTimer>
#include <QUrl>

#include "Application.h"

namespace {
constexpr int kMaxRequestBytes = 256 * 1024;
constexpr int kTimeoutMs = 20000;

const char* reasonFor(int status)
{
    switch (status) {
        case 200: return "OK";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 429: return "Too Many Requests";
        case 502: return "Bad Gateway";
        default: return "Status";
    }
}

/// Body of a chunked request, or empty when it isn't complete yet.
bool unchunk(const QByteArray& data, QByteArray* body)
{
    body->clear();
    int pos = 0;
    while (true) {
        const int lineEnd = data.indexOf("\r\n", pos);
        if (lineEnd < 0)
            return false;
        bool ok = false;
        const int size = data.mid(pos, lineEnd - pos).split(';').first().trimmed().toInt(&ok, 16);
        if (!ok)
            return false;
        pos = lineEnd + 2;
        if (size == 0)
            return true;
        if (data.size() < pos + size + 2)
            return false;
        body->append(data.mid(pos, size));
        pos += size + 2;
    }
}
}  // namespace

YggdrasilRelay::YggdrasilRelay(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network) {}

QString YggdrasilRelay::urlFor(const QString& backendUrl)
{
    if (QThread::currentThread() != qApp->thread())
        return {};
    static QPointer<YggdrasilRelay> relay;
    if (!relay) {
        relay = new YggdrasilRelay(APPLICATION->network(), qApp);
        if (!relay->listen()) {
            delete relay;
            return {};
        }
    }
    QString backend = backendUrl.trimmed();
    while (backend.endsWith(QLatin1Char('/')))
        backend.chop(1);
    relay->m_backendUrl = backend;
    return QStringLiteral("http://127.0.0.1:%1").arg(relay->m_server->serverPort());
}

bool YggdrasilRelay::listen()
{
    m_server = new QTcpServer(this);
    connect(m_server, &QTcpServer::newConnection, this, &YggdrasilRelay::onConnection);
    if (m_server->listen(QHostAddress::LocalHost, 0))
        return true;
    qWarning() << "YggdrasilRelay: can't listen on 127.0.0.1:" << m_server->errorString();
    return false;
}

void YggdrasilRelay::onConnection()
{
    while (QTcpSocket* socket = m_server->nextPendingConnection()) {
        connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        QTimer::singleShot(kTimeoutMs + 5000, socket, [socket] { socket->abort(); });
        auto buffer = std::make_shared<QByteArray>();
        auto handled = std::make_shared<bool>(false);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket, buffer, handled] {
            if (*handled)
                return;
            buffer->append(socket->readAll());
            if (buffer->size() > kMaxRequestBytes) {
                *handled = true;
                return reply(socket, { 400, "text/plain", "Too big" });
            }
            const int headerEnd = buffer->indexOf("\r\n\r\n");
            if (headerEnd < 0)
                return;

            Request request;
            const QList<QByteArray> lines = buffer->left(headerEnd).split('\n');
            const QList<QByteArray> start = lines.first().trimmed().split(' ');
            if (start.size() < 2) {
                *handled = true;
                return reply(socket, { 400, "text/plain", "Bad request" });
            }
            request.method = start.at(0);
            const QByteArray target = start.at(1);
            const int question = target.indexOf('?');
            request.path = question < 0 ? target : target.left(question);
            request.query = question < 0 ? QByteArray() : target.mid(question);
            for (int i = 1; i < lines.size(); i++) {
                const int colon = lines.at(i).indexOf(':');
                if (colon > 0)
                    request.headers.insert(lines.at(i).left(colon).trimmed().toLower(), lines.at(i).mid(colon + 1).trimmed());
            }
            const QByteArray rest = buffer->mid(headerEnd + 4);
            if (request.headers.value("transfer-encoding").toLower().contains("chunked")) {
                if (!unchunk(rest, &request.body))
                    return;
            } else {
                const int length = request.headers.value("content-length", "0").toInt();
                if (rest.size() < length)
                    return;
                request.body = rest.left(length);
            }
            *handled = true;
            handle(socket, request);
        });
    }
}

void YggdrasilRelay::handle(QTcpSocket* socket, const Request& request)
{
    QPointer<QTcpSocket> guard(socket);
    const Done done = [guard](const Answer& answer) {
        if (guard)
            reply(guard, answer);
    };
    const QString path = QString::fromUtf8(request.path);
    if (path == QLatin1String("/sessionserver/session/minecraft/join") && request.method == "POST")
        return join(request, done);
    if (path == QLatin1String("/sessionserver/session/minecraft/hasJoined") && request.method == "GET")
        return hasJoined(request, done);
    // Chat signing keys, player attributes, name lookups: only Mojang has them.
    if (path.startsWith(QLatin1String("/minecraftservices/")))
        return forward(QStringLiteral("https://api.minecraftservices.com/") + path.mid(19) + QString::fromUtf8(request.query), request, done);
    if (path.startsWith(QLatin1String("/api/")) && !path.startsWith(QLatin1String("/api/skin")) && !path.startsWith(QLatin1String("/api/yggdrasil")))
        return forward(QStringLiteral("https://api.mojang.com/") + path.mid(5) + QString::fromUtf8(request.query), request, done);
    forward(m_backendUrl + path + QString::fromUtf8(request.query), request, done);
}

void YggdrasilRelay::forward(const QString& url, const Request& request, Done done)
{
    QNetworkRequest out{ QUrl(url) };
    out.setTransferTimeout(kTimeoutMs);
    // The access token in Authorization goes on to Mojang as it is and is never kept.
    for (const char* name : { "authorization", "content-type", "accept", "user-agent" }) {
        const QByteArray value = request.headers.value(name);
        if (!value.isEmpty())
            out.setRawHeader(name, value);
    }
    if (out.rawHeader("user-agent").isEmpty())
        out.setRawHeader("User-Agent", "XylarJava");
    QNetworkReply* reply = m_network->sendCustomRequest(out, request.method, request.body);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        Answer answer;
        answer.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (answer.status == 0)
            answer.status = 502;
        answer.contentType = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray();
        answer.body = reply->readAll();
        done(answer);
    });
}

void YggdrasilRelay::join(const Request& request, Done done)
{
    forward(QStringLiteral("https://sessionserver.mojang.com/session/minecraft/join"), request, [this, request, done](const Answer& mojang) {
        if (mojang.status == 204 || mojang.status == 200)
            return done({ 204, {}, {} });
        // Not a Microsoft session: an offline account. The backend remembers it for hasJoined.
        forward(m_backendUrl + QStringLiteral("/sessionserver/session/minecraft/join"), request, done);
    });
}

void YggdrasilRelay::hasJoined(const Request& request, Done done)
{
    const QString query = QString::fromUtf8(request.query);
    Request get = request;
    get.headers.remove("authorization");
    forward(QStringLiteral("https://sessionserver.mojang.com/session/minecraft/hasJoined") + query, get,
            [this, get, query, done](const Answer& mojang) {
                if (mojang.status != 200) {
                    // Maybe an offline account that joined through XylarSkins a moment ago.
                    return forward(m_backendUrl + QStringLiteral("/sessionserver/session/minecraft/hasJoined") + query, get, done);
                }
                QJsonObject profile = QJsonDocument::fromJson(mojang.body).object();
                const QString id = profile.value("id").toString();
                if (id.isEmpty())
                    return done(mojang);
                // A Microsoft account with a XylarSkins skin plays with it here too.
                forward(m_backendUrl + QStringLiteral("/sessionserver/session/minecraft/profile/%1?unsigned=false").arg(id), get,
                        [mojang, profile, done](const Answer& xylar) mutable {
                            if (xylar.status != 200)
                                return done(mojang);
                            QJsonObject textures;
                            for (const auto& value : QJsonDocument::fromJson(xylar.body).object().value("properties").toArray()) {
                                const QJsonObject property = value.toObject();
                                if (property.value("name").toString() == QLatin1String("textures") && property.contains("signature"))
                                    textures = property;
                            }
                            if (textures.isEmpty())
                                return done(mojang);
                            QJsonArray properties;
                            for (const auto& value : profile.value("properties").toArray()) {
                                if (value.toObject().value("name").toString() != QLatin1String("textures"))
                                    properties.append(value);
                            }
                            properties.append(textures);
                            profile.insert("properties", properties);
                            done({ 200, "application/json", QJsonDocument(profile).toJson(QJsonDocument::Compact) });
                        });
            });
}

void YggdrasilRelay::reply(QTcpSocket* socket, const Answer& answer)
{
    QByteArray head = "HTTP/1.1 " + QByteArray::number(answer.status) + ' ' + reasonFor(answer.status) + "\r\n";
    if (!answer.contentType.isEmpty())
        head += "Content-Type: " + answer.contentType + "\r\n";
    head += "Content-Length: " + QByteArray::number(answer.body.size()) + "\r\nConnection: close\r\n\r\n";
    socket->write(head + answer.body);
    socket->disconnectFromHost();
}
