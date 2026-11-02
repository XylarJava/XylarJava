#include "UpnpPortMapper.h"

#include <QNetworkAccessManager>
#include <QNetworkInterface>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUdpSocket>
#include <QXmlStreamReader>

namespace {
const QHostAddress SSDP_ADDRESS(QStringLiteral("239.255.255.250"));
const quint16 SSDP_PORT = 1900;

QString xmlValue(const QByteArray& xml, const QString& element)
{
    QXmlStreamReader reader(xml);
    while (!reader.atEnd()) {
        reader.readNext();
        if (reader.isStartElement() && reader.name() == element)
            return reader.readElementText().trimmed();
    }
    return QString();
}
}  // namespace

UpnpPortMapper::UpnpPortMapper(QNetworkAccessManager* network, QObject* parent) : QObject(parent), m_network(network)
{
    m_timeout = new QTimer(this);
    m_timeout->setSingleShot(true);
    connect(m_timeout, &QTimer::timeout, this, [this] {
        if (m_controlUrl.isEmpty())
            lookupPublicAddress(tr("Your router didn't answer (UPnP may be off)."));
    });
}

UpnpPortMapper::~UpnpPortMapper()
{
    close();
}

bool UpnpPortMapper::isPrivate(const QHostAddress& address)
{
    if (address.protocol() != QAbstractSocket::IPv4Protocol)
        return address.isLoopback() || address.isLinkLocal();
    const quint32 ip = address.toIPv4Address();
    return (ip >> 24) == 10 || (ip >> 20) == (172u << 4 | 1u) || (ip >> 16) == (192u << 8 | 168u) || (ip >> 22) == (100u << 2 | 1u)
           || (ip >> 16) == (169u << 8 | 254u) || (ip >> 24) == 127;
}

QHostAddress UpnpPortMapper::localAddress()
{
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) || (flags & QNetworkInterface::IsLoopBack))
            continue;
        if (iface.type() == QNetworkInterface::Virtual)
            continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol && isPrivate(ip) && !ip.isLinkLocal())
                return ip;
        }
    }
    // No home-network address: any IPv4 address that isn't loopback, else this computer only.
    for (const QHostAddress& ip : QNetworkInterface::allAddresses()) {
        if (ip.protocol() == QAbstractSocket::IPv4Protocol && !ip.isLoopback() && !ip.isLinkLocal())
            return ip;
    }
    return QHostAddress(QHostAddress::LocalHost);
}

void UpnpPortMapper::open(quint16 port, const QString& description)
{
    if (m_busy)
        return;
    m_busy = true;
    m_port = port;
    m_description = description;
    m_localAddress = localAddress();
    if (!m_controlUrl.isEmpty()) {
        addMapping();
        return;
    }
    discover();
}

void UpnpPortMapper::discover()
{
    if (!m_socket) {
        m_socket = new QUdpSocket(this);
        connect(m_socket, &QUdpSocket::readyRead, this, &UpnpPortMapper::onDatagrams);
    }
    if (m_socket->state() != QAbstractSocket::BoundState)
        m_socket->bind(QHostAddress(QHostAddress::AnyIPv4), 0);
    const QStringList targets = { QStringLiteral("urn:schemas-upnp-org:device:InternetGatewayDevice:1"),
                                  QStringLiteral("urn:schemas-upnp-org:device:InternetGatewayDevice:2"),
                                  QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:1") };
    for (const QString& target : targets) {
        const QByteArray search = QStringLiteral("M-SEARCH * HTTP/1.1\r\nHOST: 239.255.255.250:1900\r\nMAN: \"ssdp:discover\"\r\n"
                                                 "MX: 2\r\nST: %1\r\n\r\n")
                                      .arg(target)
                                      .toLatin1();
        m_socket->writeDatagram(search, SSDP_ADDRESS, SSDP_PORT);
    }
    m_timeout->start(4000);
}

void UpnpPortMapper::onDatagrams()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(int(m_socket->pendingDatagramSize()));
        m_socket->readDatagram(datagram.data(), datagram.size());
        static const QRegularExpression location(QStringLiteral("^LOCATION:\\s*(\\S+)"),
                                                 QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        const auto match = location.match(QString::fromLatin1(datagram));
        if (match.hasMatch() && m_controlUrl.isEmpty())
            fetchDescription(QUrl(match.captured(1)));
    }
}

void UpnpPortMapper::fetchDescription(const QUrl& location)
{
    QNetworkReply* reply = m_network->get(QNetworkRequest(location));
    connect(reply, &QNetworkReply::finished, this, [this, reply, location] {
        reply->deleteLater();
        if (!m_controlUrl.isEmpty() || reply->error() != QNetworkReply::NoError)
            return;
        // Find the WANIPConnection (or WANPPPConnection) service and its control URL.
        QXmlStreamReader xml(reply->readAll());
        QString type, control;
        QString foundType, foundControl;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("service")) {
                type.clear();
                control.clear();
            } else if (xml.isStartElement() && xml.name() == QLatin1String("serviceType")) {
                type = xml.readElementText().trimmed();
            } else if (xml.isStartElement() && xml.name() == QLatin1String("controlURL")) {
                control = xml.readElementText().trimmed();
            } else if (xml.isEndElement() && xml.name() == QLatin1String("service")) {
                if ((type.contains(QLatin1String("WANIPConnection")) || type.contains(QLatin1String("WANPPPConnection"))) &&
                    foundControl.isEmpty()) {
                    foundType = type;
                    foundControl = control;
                }
            }
        }
        if (foundControl.isEmpty())
            return;
        m_serviceType = foundType;
        m_controlUrl = location.resolved(QUrl(foundControl));
        // The router told us which of our addresses it sees: use the one on its network.
        const QHostAddress router(location.host());
        for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
            for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol && entry.prefixLength() > 0 &&
                    entry.ip().isInSubnet(router, entry.prefixLength()))
                    m_localAddress = entry.ip();
            }
        }
        m_timeout->stop();
        addMapping();
    });
}

void UpnpPortMapper::soap(const QString& action, const QString& arguments, std::function<void(const QByteArray&, bool)> done)
{
    const QByteArray body = QStringLiteral("<?xml version=\"1.0\"?>"
                                           "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
                                           "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
                                           "<s:Body><u:%1 xmlns:u=\"%2\">%3</u:%1></s:Body></s:Envelope>")
                                .arg(action, m_serviceType, arguments)
                                .toUtf8();
    QNetworkRequest request(m_controlUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("text/xml; charset=\"utf-8\""));
    request.setRawHeader("SOAPAction", QStringLiteral("\"%1#%2\"").arg(m_serviceType, action).toLatin1());
    QNetworkReply* reply = m_network->post(request, body);
    connect(reply, &QNetworkReply::finished, this, [reply, done] {
        reply->deleteLater();
        done(reply->readAll(), reply->error() == QNetworkReply::NoError);
    });
}

void UpnpPortMapper::addMapping()
{
    const QString args = QStringLiteral("<NewRemoteHost></NewRemoteHost><NewExternalPort>%1</NewExternalPort><NewProtocol>TCP</NewProtocol>"
                                        "<NewInternalPort>%1</NewInternalPort><NewInternalClient>%2</NewInternalClient>"
                                        "<NewEnabled>1</NewEnabled><NewPortMappingDescription>%3</NewPortMappingDescription>"
                                        "<NewLeaseDuration>0</NewLeaseDuration>")
                             .arg(m_port)
                             .arg(m_localAddress.toString(), m_description.toHtmlEscaped());
    soap(QStringLiteral("AddPortMapping"), args, [this](const QByteArray& reply, bool ok) {
        if (!ok) {
            const QString code = xmlValue(reply, QStringLiteral("errorDescription"));
            lookupPublicAddress(code.isEmpty() ? tr("Your router refused to open the port.")
                                               : tr("Your router refused to open the port (%1).").arg(code));
            return;
        }
        m_mapped = true;
        soap(QStringLiteral("GetExternalIPAddress"), QString(), [this](const QByteArray& ipReply, bool ipOk) {
            const QString ip = ipOk ? xmlValue(ipReply, QStringLiteral("NewExternalIPAddress")) : QString();
            const QHostAddress address(ip);
            if (ip.isEmpty() || address.isNull()) {
                lookupPublicAddress(QString());
                return;
            }
            if (isPrivate(address)) {
                finish(false, QString(),
                       tr("The port is open on your router, but your internet provider shares one address between customers "
                          "(carrier-grade NAT), so friends can't reach it directly. Use a tunnel such as playit.gg, or ask your "
                          "provider for a public IP."));
                return;
            }
            finish(true, ip, tr("Port %1 is open on your router.").arg(m_port));
        });
    });
}

void UpnpPortMapper::lookupPublicAddress(const QString& upnpMessage)
{
    QNetworkReply* reply = m_network->get(QNetworkRequest(QUrl(QStringLiteral("https://api.ipify.org"))));
    connect(reply, &QNetworkReply::finished, this, [this, reply, upnpMessage] {
        reply->deleteLater();
        const QString ip = reply->error() == QNetworkReply::NoError ? QString::fromLatin1(reply->readAll()).trimmed() : QString();
        QString message = upnpMessage;
        if (message.isEmpty())
            message = tr("The router didn't say its public address.");
        message += ' ' + tr("To let friends join over the internet, forward TCP port %1 to %2 on your router, or use a tunnel such as playit.gg.")
                             .arg(m_port)
                             .arg(m_localAddress.toString());
        finish(false, QHostAddress(ip).isNull() ? QString() : ip, message);
    });
}

void UpnpPortMapper::finish(bool ok, const QString& publicAddress, const QString& message)
{
    m_busy = false;
    emit finished(ok, publicAddress, message);
}

void UpnpPortMapper::close()
{
    if (!m_mapped || m_controlUrl.isEmpty())
        return;
    m_mapped = false;
    soap(QStringLiteral("DeletePortMapping"),
         QStringLiteral("<NewRemoteHost></NewRemoteHost><NewExternalPort>%1</NewExternalPort><NewProtocol>TCP</NewProtocol>").arg(m_port),
         [](const QByteArray&, bool) {});
}
