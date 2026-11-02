#pragma once
#include <QElapsedTimer>
#include <BaseInstance.h>
#include <tools/BaseProfiler.h>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/launch/MinecraftTarget.h"

class InstanceWindow;

enum class LaunchDecision { Undecided, Continue, Abort };

class LaunchController : public Task {
    Q_OBJECT
   public:
    void executeTask() override;

    LaunchController();
    ~LaunchController() override = default;

    void setInstance(BaseInstance* instance) { m_instance = instance; }

    BaseInstance* instance() const { return m_instance; }

    void setLaunchMode(const LaunchMode mode) { m_wantedLaunchMode = mode; }

    void setOfflineName(const QString& offlineName) { m_offlineName = offlineName; }

    void setProfiler(BaseProfilerFactory* profiler) { m_profiler = profiler; }

    void setParentWidget(QWidget* widget) { m_parentWidget = widget; }

    void setTargetToJoin(MinecraftTarget::Ptr targetToJoin) { m_targetToJoin = std::move(targetToJoin); }

    void setAccountToUse(MinecraftAccountPtr accountToUse) { m_accountToUse = std::move(accountToUse); }

    QString id() const { return m_instance->id(); }
    LaunchMode launchMode() const { return m_wantedLaunchMode; }

    /// True when the launch itself failed (downloads, Java, or the game quitting while still
    /// starting up), as opposed to the user cancelling or playing for a while before a crash.
    bool failedWhileStarting() const;

    bool abort() override;

   private:
    void login();
    void launchInstance();
    void decideAccount();
    LaunchDecision decideLaunchMode();
    bool askPlayDemo() const;
    QString askOfflineName(const QString& playerName, bool* ok = nullptr) const;
    bool reauthenticateAccount(const MinecraftAccountPtr& account, const QString& reason);

   private slots:
    void readyForLaunch();

    void onSucceeded();
    void onFailed(QString reason);
    void onProgressRequested(Task* task) const;

   private:
    LaunchMode m_wantedLaunchMode = LaunchMode::Normal;
    bool m_reachedLaunch = false;
    QElapsedTimer m_startedAt;
    LaunchMode m_actualLaunchMode = LaunchMode::Normal;
    BaseProfilerFactory* m_profiler = nullptr;
    QString m_offlineName;
    BaseInstance* m_instance = nullptr;
    QWidget* m_parentWidget = nullptr;
    InstanceWindow* m_console = nullptr;
    MinecraftAccountPtr m_accountToUse = nullptr;
    AuthSessionPtr m_session = nullptr;
    LaunchTask* m_launcher = nullptr;
    MinecraftTarget::Ptr m_targetToJoin = nullptr;
};
