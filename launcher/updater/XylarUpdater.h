#pragma once

#include <QList>
#include <functional>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QWidget;

/**
 * Checks the project's GitHub releases and, when a newer version than the one
 * running is published, downloads and installs it:
 *  - AppImage: the running AppImage file is replaced in place.
 *  - Windows (portable zip): the new files are copied over after the launcher exits.
 *  - Anything else: the release page is opened.
 */
class XylarUpdater : public QObject {
    Q_OBJECT

   public:
    explicit XylarUpdater(QWidget* window, QObject* parent = nullptr);

    /// "owner/repo" on GitHub.
    void setRepository(const QString& repo) { m_repo = repo; }

    /// @param interactive show "you're up to date" / error messages too.
    void checkForUpdates(bool interactive);

    /// Numeric comparison of dotted versions ("v0.0.0.10" > "0.0.0.4"). Returns -1, 0 or 1.
    static int compareVersions(const QString& a, const QString& b);
    static QString currentVersion();

   private:
    void onReleaseReply(QNetworkReply* reply, bool interactive);
    void download(const QUrl& url, const QString& target, const QString& version, const QString& kind);
    void installAppImage(const QString& downloaded, const QString& version);
    void installWindowsZip(const QString& downloaded, const QString& version);
    void askToRestart(const QString& version, const std::function<void()>& restart);

    QWidget* m_window = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    QString m_repo = QStringLiteral("XylarJava/xylarjava");
    bool m_busy = false;
};
