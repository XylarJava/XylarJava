#pragma once

#include <QIcon>
#include <QObject>
#include <QSet>
#include <QString>

class QSystemTrayIcon;

/**
 * Desktop notifications that also show while the launcher is hidden or minimized: the
 * freedesktop notification service on Linux, the tray's balloons elsewhere.
 */
class DesktopNotifier : public QObject {
    Q_OBJECT

   public:
    /// @p icon shows on every notification; @p iconFolder is where it is written for the notification service.
    DesktopNotifier(const QIcon& icon, const QString& iconFolder, QObject* parent = nullptr);

    void notify(const QString& title, const QString& body);

   signals:
    /// The player clicked a notification.
    void clicked();

   private slots:
    void onActionInvoked(uint id, const QString& action);

   private:
    QString iconPath();
    void notifyWithTray(const QString& title, const QString& body);

    QIcon m_icon;
    QString m_iconFolder;
    QString m_iconPath;
    QSet<uint> m_sent;
    QSystemTrayIcon* m_tray = nullptr;
};
