#pragma once

#include <QFile>
#include <QJsonArray>
#include <QList>
#include <QPointer>
#include <QString>

#include <functional>
#include <memory>

#include "tasks/Task.h"

class QNetworkReply;
class QTemporaryDir;

/**
 * World backups in the cloud, on XylarJava Backups: each Social player has 500 MB, and every
 * world keeps its last 3 backups (a new one replaces the oldest). A backup is a zip of the
 * world folder.
 */
namespace WorldBackups {
struct Backup {
    QString id;
    QString instance;  ///< name of the instance it was made from
    QString world;     ///< the world's name
    qint64 size = 0;
    qint64 createdAt = 0;  ///< seconds since the epoch
};
struct Listing {
    qint64 used = 0;
    qint64 quota = 0;
    int keep = 0;
    QList<Backup> backups;  ///< newest first
};

QString backendUrl();
/// False when there's no Social player to sign the requests (no account, or not claimed yet).
bool available();

void list(QObject* context, std::function<void(bool ok, const Listing& listing, const QString& error)> done);
void remove(const QString& id, QObject* context, std::function<void(bool ok, const QString& error)> done);
}  // namespace WorldBackups

/// Zips a world folder and uploads it as a new backup.
class WorldBackupUpload : public Task {
    Q_OBJECT

   public:
    WorldBackupUpload(const QString& worldDir, const QString& instanceName, const QString& worldName);
    ~WorldBackupUpload() override;

    /// The server answered that there's no room for it (the error says how much is free).
    bool outOfSpace() const { return m_outOfSpace; }

   protected:
    void executeTask() override;
    bool abort() override;

   private:
    void beginUpload();
    void sendPart();
    void complete();
    void fail(const QString& reason);

    QString m_worldDir;
    QString m_instanceName;
    QString m_worldName;
    std::unique_ptr<QTemporaryDir> m_temp;
    QString m_zipPath;
    Task::Ptr m_zip;
    QFile m_file;
    qint64 m_size = 0;
    qint64 m_partSize = 0;
    int m_parts = 0;
    int m_next = 1;
    QString m_id;
    QJsonArray m_etags;
    QPointer<QNetworkReply> m_reply;
    bool m_aborting = false;
    bool m_outOfSpace = false;
};

/// Downloads a backup and adds it to @p worldsDir as a new world, next to the one it came from.
class WorldBackupRestore : public Task {
    Q_OBJECT

   public:
    WorldBackupRestore(const WorldBackups::Backup& backup, const QString& worldsDir);
    ~WorldBackupRestore() override;

   protected:
    void executeTask() override;
    bool abort() override;

   private:
    void install();

    WorldBackups::Backup m_backup;
    QString m_worldsDir;
    std::unique_ptr<QTemporaryDir> m_temp;
    QFile m_file;
    QPointer<QNetworkReply> m_reply;
    bool m_aborting = false;
};
