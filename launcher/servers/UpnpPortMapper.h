#pragma once

#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QUrl>
#include <functional>

class QNetworkAccessManager;
class QUdpSocket;
class QTimer;

/**
 * Asks the home router (UPnP IGD) to forward a TCP port to this computer, and finds the public
 * address friends can use. Works with most home routers that have UPnP on; when it can't
 * (UPnP off, or the provider shares one address between customers) it says why.
 */
class UpnpPortMapper : public QObject {
    Q_OBJECT
   public:
    explicit UpnpPortMapper(QNetworkAccessManager* network, QObject* parent = nullptr);
    ~UpnpPortMapper() override;

    void open(quint16 port, const QString& description);
    /// Removes the mapping made by open(); fire and forget.
    void close();

    /// This computer's address on the home network (best guess when UPnP isn't available).
    static QHostAddress localAddress();
    /// 10.x, 172.16-31.x, 192.168.x, 100.64-127.x (carrier-grade NAT) and link-local addresses.
    static bool isPrivate(const QHostAddress& address);

   signals:
    /// ok: the port is open. publicAddress may be set even when ok is false (from a lookup).
    void finished(bool ok, const QString& publicAddress, const QString& message);

   private:
    void discover();
    void onDatagrams();
    void fetchDescription(const QUrl& location);
    void soap(const QString& action, const QString& arguments, std::function<void(const QByteArray&, bool)> done);
    void addMapping();
    void lookupPublicAddress(const QString& upnpMessage);
    void finish(bool ok, const QString& publicAddress, const QString& message);

    QNetworkAccessManager* m_network;
    QUdpSocket* m_socket = nullptr;
    QTimer* m_timeout = nullptr;
    QUrl m_controlUrl;
    QString m_serviceType;
    QHostAddress m_localAddress;
    quint16 m_port = 0;
    QString m_description;
    bool m_mapped = false;
    bool m_busy = false;
};
