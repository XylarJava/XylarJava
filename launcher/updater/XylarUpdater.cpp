#include "XylarUpdater.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QPointer>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTemporaryDir>

#include "BuildConfig.h"
#include "MMCZip.h"

namespace {
QList<int> versionParts(const QString& version)
{
    QList<int> parts;
    static const QRegularExpression number(QStringLiteral("(\\d+(?:\\.\\d+)*)"));
    const QRegularExpressionMatch match = number.match(version);
    if (!match.hasMatch())
        return parts;
    for (const QString& piece : match.captured(1).split(QLatin1Char('.')))
        parts << piece.toInt();
    return parts;
}

QNetworkRequest githubRequest(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "XylarJava-Updater");
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}
}  // namespace

XylarUpdater::XylarUpdater(QWidget* window, QObject* parent) : QObject(parent), m_window(window)
{
    m_network = new QNetworkAccessManager(this);
}

QString XylarUpdater::currentVersion()
{
    const QList<int> parts = versionParts(BuildConfig.versionString());
    QStringList text;
    for (int part : parts)
        text << QString::number(part);
    return text.join(QLatin1Char('.'));
}

int XylarUpdater::compareVersions(const QString& a, const QString& b)
{
    const QList<int> left = versionParts(a);
    const QList<int> right = versionParts(b);
    const qsizetype count = qMax(left.size(), right.size());
    for (qsizetype i = 0; i < count; i++) {
        const int l = i < left.size() ? left.at(i) : 0;
        const int r = i < right.size() ? right.at(i) : 0;
        if (l != r)
            return l < r ? -1 : 1;
    }
    return 0;
}

void XylarUpdater::checkForUpdates(bool interactive)
{
    if (m_busy || m_repo.isEmpty())
        return;
    m_busy = true;
    const QUrl url(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(m_repo));
    QNetworkReply* reply = m_network->get(githubRequest(url));
    connect(reply, &QNetworkReply::finished, this, [this, reply, interactive] { onReleaseReply(reply, interactive); });
}

void XylarUpdater::onReleaseReply(QNetworkReply* reply, bool interactive)
{
    reply->deleteLater();
    m_busy = false;

    if (reply->error() != QNetworkReply::NoError) {
        qWarning() << "[Updater] Could not check for updates:" << reply->errorString();
        if (interactive)
            QMessageBox::warning(m_window, tr("Updates"), tr("Couldn't check for updates right now.\n\n%1").arg(reply->errorString()));
        return;
    }

    const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QString current = currentVersion();
    if (tag.isEmpty() || compareVersions(tag, current) <= 0) {
        qDebug() << "[Updater] Up to date:" << current << "latest:" << tag;
        if (interactive)
            QMessageBox::information(m_window, tr("Updates"), tr("You're on the latest version (%1).").arg(current));
        return;
    }

    const QString version = versionParts(tag).isEmpty() ? tag : tag.mid(tag.indexOf(QRegularExpression(QStringLiteral("\\d"))));
    const QJsonArray assets = release.value(QStringLiteral("assets")).toArray();
    const QString pageUrl = release.value(QStringLiteral("html_url")).toString();

#if defined(Q_OS_LINUX)
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    if (!appImage.isEmpty()) {
        QString arch = QSysInfo::currentCpuArchitecture();
        if (arch == QLatin1String("arm64"))
            arch = QStringLiteral("aarch64");
        QUrl best;
        for (const QJsonValue& value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (!name.endsWith(QLatin1String(".AppImage"), Qt::CaseInsensitive))
                continue;
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            if (name.contains(arch, Qt::CaseInsensitive)) {
                best = url;
                break;
            }
            if (best.isEmpty())
                best = url;
        }
        if (!best.isEmpty()) {
            download(best, appImage + QStringLiteral(".new"), version, QStringLiteral("appimage"));
            return;
        }
    }
#elif defined(Q_OS_WIN)
    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        const QString name = asset.value(QStringLiteral("name")).toString();
        if (name.endsWith(QLatin1String(".zip"), Qt::CaseInsensitive) && name.contains(QLatin1String("win"), Qt::CaseInsensitive)) {
            const QString target = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(name);
            download(QUrl(asset.value(QStringLiteral("browser_download_url")).toString()), target, version, QStringLiteral("winzip"));
            return;
        }
    }
#endif

    // No package we can install by ourselves: point the user at the release.
    const auto answer = QMessageBox::question(m_window, tr("Update available"),
                                              tr("XylarJava %1 is out (you have %2). Open the download page?").arg(version, current));
    if (answer == QMessageBox::Yes && !pageUrl.isEmpty())
        QDesktopServices::openUrl(QUrl(pageUrl));
}

void XylarUpdater::download(const QUrl& url, const QString& target, const QString& version, const QString& kind)
{
    m_busy = true;
    QPointer<QProgressDialog> progress = new QProgressDialog(tr("Downloading XylarJava %1...").arg(version), tr("Cancel"), 0, 100, m_window);
    progress->setWindowTitle(tr("Updating"));
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAttribute(Qt::WA_DeleteOnClose);

    QNetworkRequest request(url);
    request.setRawHeader("User-Agent", "XylarJava-Updater");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_network->get(request);

    auto file = new QFile(target, reply);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        reply->abort();
        if (progress)
            progress->close();
        m_busy = false;
        QMessageBox::warning(m_window, tr("Updating"), tr("Couldn't write the update to %1.").arg(target));
        return;
    }

    connect(reply, &QNetworkReply::readyRead, file, [reply, file] { file->write(reply->readAll()); });
    connect(reply, &QNetworkReply::downloadProgress, progress, [progress](qint64 received, qint64 total) {
        if (progress && total > 0)
            progress->setValue(int(received * 100 / total));
    });
    connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
    connect(reply, &QNetworkReply::finished, this, [this, reply, file, progress, target, version, kind] {
        file->write(reply->readAll());
        file->close();
        reply->deleteLater();
        if (progress)
            progress->close();
        m_busy = false;

        if (reply->error() != QNetworkReply::NoError) {
            QFile::remove(target);
            if (reply->error() != QNetworkReply::OperationCanceledError)
                QMessageBox::warning(m_window, tr("Updating"), tr("The download failed.\n\n%1").arg(reply->errorString()));
            return;
        }
        if (kind == QLatin1String("appimage"))
            installAppImage(target, version);
        else
            installWindowsZip(target, version);
    });
}

void XylarUpdater::installAppImage(const QString& downloaded, const QString& version)
{
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    QFile::setPermissions(downloaded, QFile::permissions(appImage) | QFileDevice::ExeOwner | QFileDevice::ExeUser);

    // Replacing the file is safe while it runs: the running copy keeps the old inode.
    const QString backup = appImage + QStringLiteral(".old");
    QFile::remove(backup);
    if (!QFile::rename(appImage, backup) || !QFile::rename(downloaded, appImage)) {
        QFile::rename(backup, appImage);
        QMessageBox::warning(m_window, tr("Updating"),
                             tr("Couldn't replace %1. Check that the folder is writable.").arg(QFileInfo(appImage).fileName()));
        return;
    }
    QFile::remove(backup);

    askToRestart(version, [appImage] {
        QProcess::startDetached(appImage, {});
        QCoreApplication::quit();
    });
}

void XylarUpdater::installWindowsZip(const QString& downloaded, const QString& version)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString staging = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("XylarJava-update"));
    QDir(staging).removeRecursively();
    if (!MMCZip::extractDir(downloaded, staging)) {
        QMessageBox::warning(m_window, tr("Updating"), tr("Couldn't unpack the update."));
        return;
    }
    QFile::remove(downloaded);

    // Some archives wrap everything in one top-level folder.
    QString source = staging;
    const QStringList entries = QDir(staging).entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    if (entries.size() == 1 && QFileInfo(QDir(staging).filePath(entries.first())).isDir())
        source = QDir(staging).filePath(entries.first());

    const QString exe = QCoreApplication::applicationFilePath();
    const QString script = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).filePath(QStringLiteral("xylarjava-update.bat"));
    QFile bat(script);
    if (!bat.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(m_window, tr("Updating"), tr("Couldn't prepare the update."));
        return;
    }
    const QString pid = QString::number(QCoreApplication::applicationPid());
    const QString body = QStringLiteral(
                             "@echo off\r\n"
                             ":wait\r\n"
                             "tasklist /FI \"PID eq %1\" 2>NUL | find \"%1\" >NUL && (timeout /t 1 /nobreak >NUL & goto wait)\r\n"
                             "xcopy /E /I /Y /Q \"%2\\*\" \"%3\" >NUL\r\n"
                             "start \"\" \"%4\"\r\n"
                             "rd /s /q \"%5\"\r\n"
                             "del \"%~f0\"\r\n")
                             .arg(pid, QDir::toNativeSeparators(source), QDir::toNativeSeparators(appDir), QDir::toNativeSeparators(exe),
                                  QDir::toNativeSeparators(staging));
    bat.write(body.toLocal8Bit());
    bat.close();

    askToRestart(version, [script] {
        QProcess::startDetached(QStringLiteral("cmd.exe"), { QStringLiteral("/c"), QDir::toNativeSeparators(script) });
        QCoreApplication::quit();
    });
}

void XylarUpdater::askToRestart(const QString& version, const std::function<void()>& restart)
{
    const auto answer = QMessageBox::question(m_window, tr("Update ready"),
                                              tr("XylarJava %1 is ready. Restart now to finish updating?").arg(version),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer == QMessageBox::Yes)
        restart();
}
