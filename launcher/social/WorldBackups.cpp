#include "WorldBackups.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTemporaryDir>
#include <QUrl>
#include <QtConcurrent>

#include "Application.h"
#include "FileSystem.h"
#include "MMCZip.h"
#include "SocialClient.h"
#include "XylarService.h"
#include "archive/ExportToZipTask.h"
#include "minecraft/World.h"
#include "settings/SettingsObject.h"

namespace {
const QString kService = QStringLiteral("XylarJava Backups");
// No bytes moving for this long means the connection is gone.
constexpr int kStallTimeoutMs = 60 * 1000;

std::unique_ptr<QTemporaryDir> makeTempDir()
{
    // Next to the launcher data, not in /tmp: worlds can be hundreds of MB and /tmp is often in memory.
    const QString cache = FS::PathCombine(APPLICATION->dataRoot(), QStringLiteral("cache"));
    FS::ensureFolderPathExists(cache);
    auto dir = std::make_unique<QTemporaryDir>(FS::PathCombine(cache, QStringLiteral("world-backup-XXXXXX")));
    return dir->isValid() ? std::move(dir) : nullptr;
}

QString backupUrl(const QString& id, const QString& rest = QString())
{
    return WorldBackups::backendUrl() + QStringLiteral("/v1/backups/") + id + rest;
}

QString noPlayer()
{
    return QCoreApplication::translate("WorldBackups", "Pick an account first: backups belong to your XylarJava Social player.");
}
}  // namespace

namespace WorldBackups {

QString backendUrl()
{
    return XylarService::baseUrl(APPLICATION->settings()->get("XylarBackupsUrl").toString());
}

bool available()
{
    auto social = SocialClient::current();
    return social && social->ready() && !backendUrl().isEmpty();
}

void list(QObject* context, std::function<void(bool, const Listing&, const QString&)> done)
{
    auto reply = XylarService::call(APPLICATION->network(), backendUrl() + QStringLiteral("/v1/backups"), "GET", {}, context,
                                    [done](int status, const QJsonObject& body) {
                                        if (status != 200)
                                            return done(false, {}, XylarService::messageFor(kService, status, body));
                                        Listing listing;
                                        listing.used = body.value("used").toInteger();
                                        listing.quota = body.value("quota").toInteger();
                                        listing.keep = body.value("keep").toInt();
                                        for (const auto& value : body.value("backups").toArray()) {
                                            const QJsonObject row = value.toObject();
                                            listing.backups.append({ row.value("id").toString(), row.value("instance").toString(),
                                                                     row.value("world").toString(), row.value("size").toInteger(),
                                                                     row.value("createdAt").toInteger() });
                                        }
                                        done(true, listing, QString());
                                    });
    if (!reply)
        done(false, {}, noPlayer());
}

void remove(const QString& id, QObject* context, std::function<void(bool, const QString&)> done)
{
    auto reply = XylarService::call(APPLICATION->network(), backupUrl(id), "DELETE", {}, context, [done](int status, const QJsonObject& body) {
        done(status == 200, status == 200 ? QString() : XylarService::messageFor(kService, status, body));
    });
    if (!reply)
        done(false, noPlayer());
}

}  // namespace WorldBackups

// ---------------------------------------------------------------------------------------------

WorldBackupUpload::WorldBackupUpload(const QString& worldDir, const QString& instanceName, const QString& worldName)
    : m_worldDir(worldDir), m_instanceName(instanceName.left(80)), m_worldName(worldName.left(80))
{
    setAbortable(true);
}

WorldBackupUpload::~WorldBackupUpload() = default;

void WorldBackupUpload::executeTask()
{
    if (!WorldBackups::available())
        return emitFailed(noPlayer());
    m_temp = makeTempDir();
    if (!m_temp)
        return emitFailed(tr("Couldn't make a temporary folder for the backup."));

    // world folder/... inside the zip, as the game and the "Add world" button expect it.
    const QDir dir(m_worldDir);
    QFileInfoList files;
    // session.lock is held by a running game and means nothing in a copy.
    MMCZip::collectFileListRecursively(dir.absolutePath(), nullptr, &files,
                                       [](const QFileInfo& file) { return file.fileName() == QLatin1String("session.lock"); });
    if (files.isEmpty())
        return emitFailed(tr("The world folder is empty or gone."));

    m_zipPath = FS::PathCombine(m_temp->path(), QStringLiteral("world.zip"));
    setStatus(tr("Compressing %1...").arg(m_worldName));
    m_zip.reset(new MMCZip::ExportToZipTask(m_zipPath, dir, files, dir.dirName() + QLatin1Char('/')));
    connect(m_zip.get(), &Task::progress, this, &WorldBackupUpload::setProgress);
    connect(m_zip.get(), &Task::succeeded, this, &WorldBackupUpload::beginUpload);
    connect(m_zip.get(), &Task::failed, this, [this](const QString& reason) { fail(tr("Couldn't compress the world: %1").arg(reason)); });
    connect(m_zip.get(), &Task::aborted, this, &WorldBackupUpload::emitAborted);
    m_zip->start();
}

void WorldBackupUpload::beginUpload()
{
    m_zip.reset();
    if (m_aborting)
        return emitAborted();
    m_file.setFileName(m_zipPath);
    if (!m_file.open(QIODevice::ReadOnly))
        return fail(tr("Couldn't read the compressed world."));
    m_size = m_file.size();

    setStatus(tr("Uploading %1...").arg(m_worldName));
    setProgress(0, m_size);
    const QJsonObject body{ { "instance", m_instanceName }, { "world", m_worldName }, { "size", m_size } };
    m_reply = XylarService::call(APPLICATION->network(), WorldBackups::backendUrl() + QStringLiteral("/v1/backups"), "POST",
                                 QJsonDocument(body).toJson(QJsonDocument::Compact), this, [this](int status, const QJsonObject& reply) {
                                     if (m_aborting)
                                         return emitAborted();
                                     if (status != 201) {
                                         m_outOfSpace = status == 413;
                                         return fail(XylarService::messageFor(kService, status, reply));
                                     }
                                     m_id = reply.value("id").toString();
                                     m_partSize = reply.value("partSize").toInteger();
                                     m_parts = reply.value("parts").toInt();
                                     if (m_id.isEmpty() || m_partSize <= 0 || m_parts <= 0)
                                         return fail(tr("XylarJava Backups sent an answer we don't understand."));
                                     sendPart();
                                 });
    if (!m_reply)
        fail(noPlayer());
}

void WorldBackupUpload::sendPart()
{
    if (m_next > m_parts)
        return complete();
    const qint64 offset = (m_next - 1) * m_partSize;
    if (!m_file.seek(offset))
        return fail(tr("Couldn't read the compressed world."));
    const QByteArray part = m_file.read(std::min(m_partSize, m_size - offset));
    if (part.size() != std::min(m_partSize, m_size - offset))
        return fail(tr("Couldn't read the compressed world."));

    const int n = m_next;
    m_reply = XylarService::call(
        APPLICATION->network(), backupUrl(m_id, QStringLiteral("/parts/%1").arg(n)), "PUT", part, this,
        [this, n](int status, const QJsonObject& reply) {
            if (m_aborting)
                return emitAborted();
            if (status != 200)
                return fail(XylarService::messageFor(kService, status, reply));
            m_etags.append(QJsonObject{ { "n", n }, { "etag", reply.value("etag").toString() } });
            m_next++;
            sendPart();
        },
        "application/octet-stream", kStallTimeoutMs);
    if (!m_reply)
        return fail(noPlayer());
    connect(m_reply, &QNetworkReply::uploadProgress, this, [this, offset](qint64 sent, qint64) { setProgress(offset + sent, m_size); });
}

void WorldBackupUpload::complete()
{
    m_file.close();
    setStatus(tr("Finishing the backup of %1...").arg(m_worldName));
    m_reply = XylarService::call(APPLICATION->network(), backupUrl(m_id, QStringLiteral("/complete")), "POST",
                                 QJsonDocument(QJsonObject{ { "parts", m_etags } }).toJson(QJsonDocument::Compact), this,
                                 [this](int status, const QJsonObject& reply) {
                                     if (m_aborting)
                                         return emitAborted();
                                     if (status != 201)
                                         return fail(XylarService::messageFor(kService, status, reply));
                                     m_temp.reset();
                                     emitSucceeded();
                                 });
    if (!m_reply)
        fail(noPlayer());
}

void WorldBackupUpload::fail(const QString& reason)
{
    m_file.close();
    m_temp.reset();
    // An unfinished upload would count against the space until the server drops it.
    if (!m_id.isEmpty())
        XylarService::call(APPLICATION->network(), backupUrl(m_id), "DELETE", {}, APPLICATION->network(), nullptr);
    m_id.clear();
    emitFailed(reason);
}

bool WorldBackupUpload::abort()
{
    if (!isRunning())
        return false;
    m_aborting = true;
    if (m_zip)
        return m_zip->abort();
    if (!m_id.isEmpty())
        XylarService::call(APPLICATION->network(), backupUrl(m_id), "DELETE", {}, APPLICATION->network(), nullptr);
    m_id.clear();
    if (m_reply)
        m_reply->abort();  // its answer (status 0) ends the task as aborted
    else
        emitAborted();
    return true;
}

// ---------------------------------------------------------------------------------------------

WorldBackupRestore::WorldBackupRestore(const WorldBackups::Backup& backup, const QString& worldsDir)
    : m_backup(backup), m_worldsDir(worldsDir)
{
    setAbortable(true);
}

WorldBackupRestore::~WorldBackupRestore() = default;

void WorldBackupRestore::executeTask()
{
    auto social = SocialClient::current();
    QNetworkRequest request{ QUrl(backupUrl(m_backup.id, QStringLiteral("/file"))) };
    if (!social || !social->authorize(request))
        return emitFailed(noPlayer());
    m_temp = makeTempDir();
    if (!m_temp)
        return emitFailed(tr("Couldn't make a temporary folder for the download."));
    m_file.setFileName(FS::PathCombine(m_temp->path(), QStringLiteral("world.zip")));
    if (!m_file.open(QIODevice::WriteOnly))
        return emitFailed(tr("Couldn't save the download."));

    setStatus(tr("Downloading %1...").arg(m_backup.world));
    setProgress(0, m_backup.size);
    request.setTransferTimeout(kStallTimeoutMs);
    m_reply = APPLICATION->network()->get(request);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] {
        if (m_file.write(m_reply->readAll()) < 0)
            m_reply->abort();
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64) { setProgress(received, m_backup.size); });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        auto reply = m_reply;
        reply->deleteLater();
        m_file.write(reply->readAll());
        m_file.close();
        if (m_aborting)
            return emitAborted();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            // The error came in place of the file.
            QJsonObject error;
            if (m_file.open(QIODevice::ReadOnly)) {
                error = QJsonDocument::fromJson(m_file.read(64 * 1024)).object();
                m_file.close();
            }
            return emitFailed(XylarService::messageFor(kService, status, error));
        }
        if (m_file.size() != m_backup.size)
            return emitFailed(tr("The download stopped early. Try again."));
        install();
    });
}

void WorldBackupRestore::install()
{
    setStatus(tr("Adding %1 to your worlds...").arg(m_backup.world));
    setAbortable(false);
    const QString zip = m_file.fileName();
    const QString name = tr("%1 (backup %2)")
                             .arg(m_backup.world, QLocale().toString(QDateTime::fromSecsSinceEpoch(m_backup.createdAt), QLocale::ShortFormat));
    const QString worldsDir = m_worldsDir;
    auto watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher] {
        watcher->deleteLater();
        m_temp.reset();
        if (watcher->result())
            emitSucceeded();
        else
            emitFailed(tr("The backup isn't a world we can read."));
    });
    watcher->setFuture(QtConcurrent::run([zip, worldsDir, name] {
        World world{ QFileInfo(zip) };
        return world.isValid() && FS::ensureFolderPathExists(worldsDir) && world.install(worldsDir, name);
    }));
}

bool WorldBackupRestore::abort()
{
    if (!isRunning() || !canAbort())
        return false;
    m_aborting = true;
    if (m_reply)
        m_reply->abort();
    else
        emitAborted();
    return true;
}
