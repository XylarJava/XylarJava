// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <QJsonObject>
#include <QProcess>
#include <QLockFile>
#include <functional>
#include "launch/LaunchStep.h"
#include "minecraft/auth/AuthSession.h"
#include "minecraft/launch/MinecraftTarget.h"

class QNetworkReply;
class RustLaunch : public LaunchStep {
    Q_OBJECT
 public:
    RustLaunch(LaunchTask* parent, AuthSessionPtr session, MinecraftTarget::Ptr target);
    void executeTask() override;
    bool canAbort() const override { return true; }
    bool abort() override;
    void finalize() override;
    void proceed() override;
 private:
    void getJson(const QUrl& url, std::function<void(QJsonObject)> next, bool authenticated = false);
    void installEngine();
    void probeEngine();
    void fetchMinecraft();
    void fetchGameFiles(const QJsonObject& manifest);
    void ensureJava();
    void prepare();
    void launch();
    void runProgram(const QString& command, const QStringList& arguments, std::function<void(QByteArray)> next, int timeoutMs = 120000, std::function<void()> retry = {});
    void followTask(Task::Ptr task, std::function<void()> next);
    bool addDownload(class NetJob* job, const QString& url, const QString& path, const QString& hash, bool sha256 = false);
    QString engineBinary(const QString& name) const;
    std::unique_ptr<QLockFile> m_installLock;
    AuthSessionPtr m_session;
    MinecraftTarget::Ptr m_target;
    QString m_engine, m_files, m_prepared, m_javaHome;
    QProcess* m_process = nullptr;
    QNetworkReply* m_reply = nullptr;
    Task::Ptr m_task;
    bool m_waiting = false;
    bool m_aborted = false;
};
