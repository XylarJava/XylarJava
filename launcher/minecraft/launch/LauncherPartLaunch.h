#pragma once

#include <LoggedProcess.h>
#include <launch/LaunchStep.h>
#include <minecraft/auth/AuthSession.h>

#include "MinecraftTarget.h"

class LauncherPartLaunch : public LaunchStep {
    Q_OBJECT
   public:
    explicit LauncherPartLaunch(LaunchTask* parent);
    virtual ~LauncherPartLaunch() = default;

    virtual void executeTask();
    virtual bool abort();
    virtual void proceed();
    virtual bool canAbort() const { return true; }
    void setWorkingDirectory(const QString& wd);
    void setAuthSession(AuthSessionPtr session) { m_session = session; }

    void setTargetToJoin(MinecraftTarget::Ptr targetToJoin) { m_targetToJoin = std::move(targetToJoin); }

   private slots:
    void on_state(LoggedProcess::State state);

   private:
    LoggedProcess m_process;
    QString m_command;
    AuthSessionPtr m_session;
    QString m_launchScript;
    MinecraftTarget::Ptr m_targetToJoin;

    bool mayProceed = false;
    // Applet era versions start through NewLaunch, which reads the launch script on stdin.
    bool m_viaNewLaunch = false;
};
