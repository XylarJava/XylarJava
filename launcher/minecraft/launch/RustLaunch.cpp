// SPDX-License-Identifier: GPL-3.0-only
#include "RustLaunch.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>
#include "Application.h"
#include "FileSystem.h"
#include "archive/ArchiveReader.h"
#include "archive/ExtractZipTask.h"
#include "java/download/ArchiveDownloadTask.h"
#include "launch/LaunchTask.h"
#include "net/ChecksumValidator.h"
#include "net/NetJob.h"
#include "settings/SettingsObject.h"

namespace {
QJsonObject readObject(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()).object() : QJsonObject();
}
bool save(const QString& path, const QByteArray& data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
bool validHash(const QString& hash, int length)
{
    return QRegularExpression(QString("^[0-9a-fA-F]{%1}$").arg(length)).match(hash).hasMatch();
}
}

RustLaunch::RustLaunch(LaunchTask* parent, AuthSessionPtr session, MinecraftTarget::Ptr target)
    : LaunchStep(parent), m_session(std::move(session)), m_target(std::move(target))
{}
QString RustLaunch::engineBinary(const QString& name) const
{
#ifdef Q_OS_WIN
    return m_engine + "/" + name + ".exe";
#else
    return m_engine + "/" + name;
#endif
}
void RustLaunch::executeTask()
{
    if (!m_session || m_session->user_type != "msa" || m_session->launchMode != LaunchMode::Normal || m_session->access_token.isEmpty()) {
        emitFailed(tr("A Microsoft account that owns Minecraft: Java Edition is required for Rust."));
        return;
    }
    emit progressReportingRequest();
    setStatus(tr("Checking Minecraft ownership…"));
    getJson(QUrl("https://api.minecraftservices.com/minecraft/profile"), [this](const QJsonObject& profile) {
        if (profile["id"].toString().remove('-') != QString(m_session->uuid).remove('-') || profile["name"] != m_session->player_name) {
            emitFailed(tr("The Microsoft session does not match this player. Sign in again."));
            return;
        }
        getJson(QUrl("https://api.minecraftservices.com/entitlements/mcstore"), [this](const QJsonObject& entitlements) {
            bool owns = false;
            for (const auto& item : entitlements["items"].toArray()) {
                const auto name = item.toObject()["name"].toString();
                owns |= name == "game_minecraft" || name == "product_minecraft";
            }
            if (!owns) {
                emitFailed(tr("A Microsoft account that owns Minecraft: Java Edition is required for Rust."));
                return;
            }
            installEngine();
        }, true);
    }, true);
}
void RustLaunch::getJson(const QUrl& url, std::function<void(QJsonObject)> next, bool authenticated)
{
    if (m_aborted) return;
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "XylarJava");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, authenticated ? QNetworkRequest::ManualRedirectPolicy : QNetworkRequest::NoLessSafeRedirectPolicy);
    if (authenticated) request.setRawHeader("Authorization", "Bearer " + m_session->access_token.toUtf8());
    m_reply = APPLICATION->network()->get(request);
    auto* reply = m_reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, next = std::move(next)] {
        const auto bytes = reply->readAll();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto object = QJsonDocument::fromJson(bytes).object();
        if (m_reply == reply) m_reply = nullptr;
        reply->deleteLater();
        if (m_aborted) return;
        if (reply->error() != QNetworkReply::NoError || code != 200 || object.isEmpty()) {
            emitFailed(tr("Rust download or account check failed (HTTP %1, %2). Try again.").arg(code).arg(reply->url().host()));
            return;
        }
        next(object);
    });
}
void RustLaunch::followTask(Task::Ptr task, std::function<void()> next)
{
    m_task = task;
    connect(task.get(), &Task::status, this, &RustLaunch::setStatus);
    connect(task.get(), &Task::progress, this, &RustLaunch::setProgress);
    connect(task.get(), &Task::failed, this, [this](const QString& error) { if (!m_aborted) emitFailed(error); });
    connect(task.get(), &Task::succeeded, this, [this, next = std::move(next)] { if (!m_aborted) next(); });
    task->start();
}
bool RustLaunch::addDownload(NetJob* job, const QString& url, const QString& path, const QString& hash, bool sha256)
{
    const QUrl source(url);
    if (source.scheme() != "https" || !validHash(hash, sha256 ? 64 : 40)) {
        emitFailed(tr("Rust download metadata has no valid checksum or HTTPS address."));
        return false;
    }
    const auto algorithm = sha256 ? QCryptographicHash::Sha256 : QCryptographicHash::Sha1;
    QFile existing(path);
    if (existing.open(QIODevice::ReadOnly)) {
        QCryptographicHash digest(algorithm);
        digest.addData(&existing);
        if (digest.result().toHex() == hash.toLatin1().toLower()) return true;
        existing.close();
    }
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        emitFailed(tr("Cannot create the Rust download folder: %1").arg(path));
        return false;
    }
    auto action = Net::Download::makeFile(source, path);
    action->addValidator(new Net::ChecksumValidator(algorithm, hash));
    job->addNetAction(action);
    return true;
}
void RustLaunch::installEngine()
{
#if !defined(Q_OS_WIN) && !defined(Q_OS_LINUX)
    emitFailed(tr("Rust beta is currently supported on Windows x64 and Linux x86_64."));
    return;
#endif
    if (QSysInfo::currentCpuArchitecture() != "x86_64") {
        emitFailed(tr("Rust beta currently requires an x86_64 computer."));
        return;
    }
    const QString root = APPLICATION->root() + "/native-rust";
    QDir().mkpath(root);
    m_installLock = std::make_unique<QLockFile>(root + "/prepare.lock");
    if (!m_installLock->tryLock()) {
        emitFailed(tr("Another Rust instance is preparing resources. Try again when it finishes.")); return;
    }
    const auto installed = readObject(root + "/installed.json");
    const QString digest = installed["digest"].toString();
    if (validHash(digest, 64) && !m_parent->instance()->settings()->get("NativeRustRepair").toBool()) {
        m_engine = root + "/" + digest;
        if (QFileInfo::exists(engineBinary("horizon-client")) && QFileInfo::exists(engineBinary("horizon-prepare"))) {
            probeEngine();
            return;
        }
    }
    setStatus(tr("Downloading Rust 26.3 Beta…"));
    getJson(QUrl("https://api.github.com/repos/horizon-rust/horizon/releases/tags/nightly"), [this, root](const QJsonObject& release) {
#ifdef Q_OS_WIN
        const QString assetName = "horizon-windows-x64.zip";
#else
        const QString assetName = "horizon-linux-x86_64.tar.gz";
#endif
        for (const auto& item : release["assets"].toArray()) {
            const auto asset = item.toObject();
            if (asset["name"] != assetName) continue;
            const QString digest = asset["digest"].toString().remove("sha256:");
            const QUrl url(asset["browser_download_url"].toString());
            if (!validHash(digest, 64) || url.host() != "github.com" ||
                !url.path().startsWith("/horizon-rust/horizon/releases/download/")) break;
            m_engine = root + "/" + digest;
            const QString archive = root + "/" + assetName;
            auto download = makeShared<NetJob>(tr("Download Rust engine"), APPLICATION->network());
            if (!addDownload(download.get(), url.toString(), archive, digest, true)) return;
            followTask(download, [this, archive, root, digest] {
                MMCZip::ArchiveReader reader(archive);
                if (!reader.collectFiles() || reader.getFiles().isEmpty()) {
                    emitFailed(tr("The Rust engine archive cannot be opened.")); return;
                }
                const auto files = reader.getFiles();
                QString prefix = files.first().section('/', 0, 0);
                for (const auto& name : files)
                    if (!name.startsWith(prefix + '/')) { prefix.clear(); break; }
                auto extract = makeShared<MMCZip::ExtractZipTask>(archive, QDir(m_engine), prefix);
                followTask(extract, [this, root, digest] {
                    QFile::setPermissions(engineBinary("horizon-client"), QFile::permissions(engineBinary("horizon-client")) | QFile::ExeOwner);
                    QFile::setPermissions(engineBinary("horizon-prepare"), QFile::permissions(engineBinary("horizon-prepare")) | QFile::ExeOwner);
                    if (!save(root + "/installed.json", QJsonDocument(QJsonObject{{"digest", digest}}).toJson())) {
                        emitFailed(tr("Cannot save the Rust installation record.")); return;
                    }
                    probeEngine();
                });
            });
            return;
        }
        emitFailed(tr("A verified Rust build for this system is unavailable. Try again later."));
    });
}
void RustLaunch::probeEngine()
{
    setStatus(tr("Checking Rust launch compatibility…"));
    runProgram(engineBinary("horizon-prepare"), {"--launch-api"}, [this](const QByteArray& output) {
        const auto api = QJsonDocument::fromJson(output).object();
        if (!api["launch_api"].toString().startsWith("0.1.") || api["minecraft"] != "26.3" || api["java_major"].toInt() != 25) {
            emitFailed(tr("This Rust build requires a different launch API or Minecraft version. Update XylarJava before using it.")); return;
        }
        runProgram(engineBinary("horizon-client"), {"--launch-api"}, [this](const QByteArray& clientOutput) {
            const auto clientApi = QJsonDocument::fromJson(clientOutput).object();
            if (!clientApi["launch_api"].toString().startsWith("0.1.")) {
                emitFailed(tr("This Rust build does not support the required launch API.")); return;
            }
            m_files = APPLICATION->root() + "/native-rust/minecraft";
            m_prepared = m_engine + "/prepared/26.3";
            if (QFileInfo::exists(m_prepared + "/.xylar-ready") && !m_parent->instance()->settings()->get("NativeRustRepair").toBool()) {
                runProgram(engineBinary("horizon-prepare"), {"--check", "--output", m_prepared}, [this](const QByteArray&) {
                    m_waiting = true;
                    m_installLock.reset();
                    emit readyForLaunch();
                }, 120000, [this] { fetchMinecraft(); });
            } else fetchMinecraft();
        });
    });
}
void RustLaunch::fetchMinecraft()
{
    setStatus(tr("Preparing Minecraft 26.3 files…"));
    getJson(QUrl("https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"), [this](const QJsonObject& manifest) {
        for (const auto& value : manifest["versions"].toArray()) {
            const auto version = value.toObject();
            if (version["id"] != "26.3") continue;
            const QString path = m_files + "/versions/26.3/26.3.json";
            auto download = makeShared<NetJob>(tr("Download Minecraft version"), APPLICATION->network());
            if (!addDownload(download.get(), version["url"].toString(), path, version["sha1"].toString())) return;
            followTask(download, [this, path] { fetchGameFiles(readObject(path)); });
            return;
        }
        emitFailed(tr("Minecraft 26.3 is unavailable from the official version service."));
    });
}
void RustLaunch::fetchGameFiles(const QJsonObject& manifest)
{
    if (manifest["id"] != "26.3") { emitFailed(tr("The Minecraft version file is invalid.")); return; }
    auto files = makeShared<NetJob>(tr("Download Minecraft 26.3 resources"), APPLICATION->network(), 4);
    const auto downloads = manifest["downloads"].toObject();
    for (const auto& kind : {QStringLiteral("client"), QStringLiteral("server")}) {
        const auto info = downloads[kind].toObject();
        const QString target = kind == "client" ? m_files + "/versions/26.3/26.3.jar" : m_files + "/server.jar";
        if (!addDownload(files.get(), info["url"].toString(), target, info["sha1"].toString())) return;
    }
    for (const auto& value : manifest["libraries"].toArray()) {
        const auto info = value.toObject()["downloads"].toObject()["artifact"].toObject();
        if (info.isEmpty()) continue;
        const QString relative = QDir::cleanPath(info["path"].toString());
        if (relative.isEmpty() || QDir::isAbsolutePath(relative) || relative.startsWith("..") || relative.contains(':') || relative.contains('\\')) {
            emitFailed(tr("The Minecraft library path is invalid.")); return;
        }
        if (!addDownload(files.get(), info["url"].toString(), m_files + "/libraries/" + relative, info["sha1"].toString())) return;
    }
    const auto index = manifest["assetIndex"].toObject();
    const QString indexId = index["id"].toString();
    if (!QRegularExpression("^[a-zA-Z0-9_.-]+$").match(indexId).hasMatch()) { emitFailed(tr("Invalid asset index.")); return; }
    const QString indexPath = m_files + "/assets/indexes/" + indexId + ".json";
    if (!addDownload(files.get(), index["url"].toString(), indexPath, index["sha1"].toString())) return;
    followTask(files, [this, indexPath] {
        auto assets = makeShared<NetJob>(tr("Download game sounds and assets"), APPLICATION->network(), 4);
        const auto objects = readObject(indexPath)["objects"].toObject();
        if (objects.isEmpty()) { emitFailed(tr("The Minecraft asset index is empty.")); return; }
        for (const auto& value : objects) {
            const auto hash = value.toObject()["hash"].toString();
            if (!validHash(hash, 40)) { emitFailed(tr("Invalid asset checksum.")); return; }
            const QString path = hash.left(2) + "/" + hash;
            if (!addDownload(assets.get(), "https://resources.download.minecraft.net/" + path, m_files + "/assets/objects/" + path, hash)) return;
        }
        followTask(assets, [this] { ensureJava(); });
    });
}
void RustLaunch::ensureJava()
{
    const QString root = APPLICATION->root() + "/native-rust/java25";
    if (QFileInfo::exists(root + "/bin/java") || QFileInfo::exists(root + "/bin/java.exe")) {
        m_javaHome = root; prepare(); return;
    }
#ifdef Q_OS_WIN
    const QString os = "windows";
#else
    const QString os = "linux";
#endif
    // Adoptium supplies a JRE for resource exporters; the native game itself needs no Java.
    QUrl url("https://api.adoptium.net/v3/assets/latest/25/hotspot?architecture=x64&image_type=jre&os=" + os + "&vendor=eclipse");
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    auto* reply = APPLICATION->network()->get(request);
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, root] {
        reply->deleteLater();
        if (m_reply == reply) m_reply = nullptr;
        if (m_aborted) return;
        const auto list = QJsonDocument::fromJson(reply->readAll()).array();
        const auto package = (list.isEmpty() ? QJsonObject() : list.first().toObject())["binary"].toObject()["package"].toObject();
        if (reply->error() != QNetworkReply::NoError || !validHash(package["checksum"].toString(), 64) ||
            QUrl(package["link"].toString()).scheme() != "https") {
            emitFailed(tr("Could not download Java 25 for preparing Rust resources.")); return;
        }
        m_javaHome = root;
        followTask(makeShared<Java::ArchiveDownloadTask>(QUrl(package["link"].toString()), root, "sha256", package["checksum"].toString()), [this] { prepare(); });
    });
}
void RustLaunch::prepare()
{
    setStatus(tr("Preparing Rust 26.3 resources. The first launch can take several minutes…"));
    runProgram(engineBinary("horizon-prepare"), {"--prepare", "--server-jar", m_files + "/server.jar", "--minecraft-dir", m_files,
        "--java-home", m_javaHome, "--output", m_prepared, "--complete", "--progress", "json"}, [this](const QByteArray&) {
        if (!save(m_prepared + "/.xylar-ready", "26.3\n")) {
            emitFailed(tr("Cannot save the Rust preparation record.")); return;
        }
        m_parent->instance()->settings()->set("NativeRustRepair", false);
        m_waiting = true;
        m_installLock.reset();
        setStatus(tr("Rust 26.3 is ready."));
        emit readyForLaunch();
    }, 20 * 60 * 1000);
}
void RustLaunch::runProgram(const QString& command, const QStringList& arguments, std::function<void(QByteArray)> next, int timeoutMs, std::function<void()> retry)
{
    auto* process = new QProcess(this);
    m_process = process;
    process->setWorkingDirectory(m_engine);
    auto environment = m_parent->instance()->createLaunchEnvironment();
    environment.remove("JAVA_HOME");
    process->setProcessEnvironment(environment);
    auto buffer = std::make_shared<QByteArray>();
    auto* timer = new QTimer(process);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, process, [process] { process->kill(); });
    connect(process, &QProcess::readyReadStandardOutput, this, [process, buffer] {
        buffer->append(process->readAllStandardOutput());
        if (buffer->size() > 2 * 1024 * 1024) buffer->remove(0, buffer->size() - 2 * 1024 * 1024);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        emit logLines(QString::fromUtf8(process->readAllStandardError()).split('\n', Qt::SkipEmptyParts), MessageLevel::Launcher);
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !m_aborted) emitFailed(tr("Rust could not start: %1").arg(process->errorString()));
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
        [this, process, timer, buffer, next = std::move(next), retry = std::move(retry)](int code, QProcess::ExitStatus status) {
            timer->stop(); process->deleteLater();
            if (m_process == process) m_process = nullptr;
            if (m_aborted) return;
            if (code != 0 || status != QProcess::NormalExit) {
                if (retry) { retry(); return; }
                emit logLine(QString::fromUtf8(buffer->right(4096)), MessageLevel::Error);
                emitFailed(tr("Rust preparation failed (exit %1). See the instance log for details.").arg(code)); return;
            }
            next(*buffer);
        });
    timer->start(timeoutMs);
    process->start(command, arguments);
}
void RustLaunch::proceed()
{
    if (!m_waiting || m_aborted) return;
    m_waiting = false;
    launch();
}
void RustLaunch::launch()
{
    auto* instance = m_parent->instance();
    QDir().mkpath(instance->gameRoot());
    auto* process = new QProcess(this);
    m_process = process;
    process->setWorkingDirectory(instance->gameRoot());
    process->setProcessEnvironment(instance->createLaunchEnvironment());
    QStringList arguments{"--resources", m_prepared, "--game-dir", instance->gameRoot(), "--home", instance->gameRoot(), "--session-stdin"};
    if (m_target) {
        if (!m_target->world.isEmpty()) arguments << "--quickPlaySingleplayer" << m_target->world;
        else arguments << "--quickPlayMultiplayer" << (m_target->address.contains(':') ? "[" + m_target->address + "]:" : m_target->address + ":") + QString::number(m_target->port);
    }
    connect(process, &QProcess::started, this, [this, process] {
        m_parent->setPid(process->processId());
        const QJsonObject session{{"name", m_session->player_name}, {"uuid", m_session->uuid}, {"access_token", m_session->access_token}, {"user_type", "msa"}};
        process->write(QJsonDocument(session).toJson(QJsonDocument::Compact));
        process->closeWriteChannel();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] { emit logLines(QString::fromUtf8(process->readAllStandardOutput()).split('\n', Qt::SkipEmptyParts), MessageLevel::StdOut); });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] { emit logLines(QString::fromUtf8(process->readAllStandardError()).split('\n', Qt::SkipEmptyParts), MessageLevel::StdErr); });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !m_aborted) emitFailed(tr("Rust could not start: %1").arg(process->errorString()));
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this](int code, QProcess::ExitStatus status) {
        if (m_aborted) return;
        if (code == 0 && status == QProcess::NormalExit) emitSucceeded();
        else emitFailed(tr("Rust exited with code %1. See the instance log for details.").arg(code));
    });
    process->start(engineBinary("horizon-client"), arguments);
}
bool RustLaunch::abort()
{
    m_aborted = true;
    if (m_reply) m_reply->abort();
    if (m_task && m_task->isRunning()) m_task->abort();
    if (m_process) m_process->kill();
    emitAborted();
    return true;
}
void RustLaunch::finalize()
{
    m_installLock.reset();
    if (m_process && m_process->state() != QProcess::NotRunning) m_process->kill();
}
