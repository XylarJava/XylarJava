#include "DesktopNotifier.h"

#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVariantMap>

#ifdef WITH_QTDBUS
#include <QtDBus/QtDBus>
#endif

namespace {
constexpr auto kService = "org.freedesktop.Notifications";
constexpr auto kPath = "/org/freedesktop/Notifications";
constexpr auto kInterface = "org.freedesktop.Notifications";
}  // namespace

DesktopNotifier::DesktopNotifier(const QIcon& icon, const QString& iconFolder, QObject* parent)
    : QObject(parent), m_icon(icon.isNull() ? QGuiApplication::windowIcon() : icon), m_iconFolder(iconFolder)
{
#ifdef WITH_QTDBUS
    QDBusConnection::sessionBus().connect(kService, kPath, kInterface, QStringLiteral("ActionInvoked"), this,
                                          SLOT(onActionInvoked(uint, QString)));
#endif
}

QString DesktopNotifier::iconPath()
{
    // The notification service wants a file, so the icon is written out once per run.
    if (m_iconPath.isEmpty() && !m_icon.isNull()) {
        const QString path = QDir(m_iconFolder).filePath(QStringLiteral("xylarjava-notification.png"));
        if (QDir().mkpath(m_iconFolder) && m_icon.pixmap(128, 128).save(path, "PNG"))
            m_iconPath = path;
    }
    return m_iconPath;
}

void DesktopNotifier::notify(const QString& title, const QString& body)
{
#ifdef WITH_QTDBUS
    QDBusInterface service(kService, kPath, kInterface, QDBusConnection::sessionBus());
    if (service.isValid()) {
        QVariantMap hints;
        hints.insert(QStringLiteral("desktop-entry"), QStringLiteral("io.xylarjava.XylarJava"));
        const QDBusReply<uint> reply =
            service.call(QStringLiteral("Notify"), QStringLiteral("XylarJava"), uint(0), iconPath(), title, body,
                         QStringList{ QStringLiteral("default"), tr("Open") }, hints, int(8000));
        if (reply.isValid()) {
            m_sent.insert(reply.value());
            return;
        }
    }
#endif
    notifyWithTray(title, body);
}

void DesktopNotifier::onActionInvoked(uint id, const QString& action)
{
    if (!m_sent.remove(id))
        return;  // another app's notification
    if (action == QLatin1String("default"))
        emit clicked();
}

void DesktopNotifier::notifyWithTray(const QString& title, const QString& body)
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;
    if (!m_tray) {
        m_tray = new QSystemTrayIcon(m_icon, this);
        m_tray->setToolTip(QStringLiteral("XylarJava"));
        connect(m_tray, &QSystemTrayIcon::messageClicked, this, &DesktopNotifier::clicked);
        connect(m_tray, &QSystemTrayIcon::activated, this, &DesktopNotifier::clicked);
    }
    m_tray->show();
    m_tray->showMessage(title, body, m_icon, 8000);
    // The icon only stays in the tray while the message can be clicked.
    QTimer::singleShot(15000, m_tray, &QSystemTrayIcon::hide);
}
