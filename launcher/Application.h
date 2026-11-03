#pragma once

#include <memory>

#include <QApplication>
#include <QHash>
#include <QSet>
#include <QDateTime>
#include <QDebug>
#include <QFlag>
#include <QIcon>
#include <QMutex>
#include <QUrl>

#include "QObjectPtr.h"

namespace InstanceRepair {
struct Diagnosis;
}

#include "minecraft/auth/MinecraftAccount.h"

class LaunchController;
class LocalPeer;
class InstanceWindow;
class MainWindow;
class ViewLogWindow;
class SetupWizard;
class GenericPageProvider;
class BasePageProvider;
class QFile;
class HttpMetaCache;
class SettingsObject;
class InstanceList;
class AccountList;
class IconList;
class QNetworkAccessManager;
class JavaInstallList;
class ExternalUpdater;
class BaseProfilerFactory;
class BaseDetachedToolFactory;
class TranslationsModel;
class ITheme;
class MCEditTool;
class ThemeManager;
class IconTheme;
class BaseInstance;

class LogModel;

struct MinecraftTarget;
class MinecraftAccount;
class XylarSkinClient;

namespace Meta {
class Index;
}

#if defined(APPLICATION)
#undef APPLICATION
#endif
#define APPLICATION (static_cast<Application*>(QCoreApplication::instance()))

// Used for checking if is a test
#if defined(APPLICATION_DYN)
#undef APPLICATION_DYN
#endif
#define APPLICATION_DYN (dynamic_cast<Application*>(QCoreApplication::instance()))

class Application : public QApplication {
    Q_OBJECT
   public:
    enum Status { StartingUp, Failed, Succeeded, Initialized };

    enum Capability {
        None = 0,

        SupportsMSA = 1 << 0,
        SupportsFlame = 1 << 1,
        SupportsGameMode = 1 << 2,
        SupportsMangoHud = 1 << 3,
    };
    Q_DECLARE_FLAGS(Capabilities, Capability)

   public:
    Application(int& argc, char** argv);
    virtual ~Application();

    bool event(QEvent* event) override;

    SettingsObject* settings() const { return m_settings.get(); }
    /// The pages of the global settings, for showing them inside the main window.
    BasePageProvider* globalSettingsProvider();

    qint64 timeSinceStart() const { return m_startTime.msecsTo(QDateTime::currentDateTime()); }

    QIcon logo();

    ThemeManager* themeManager() { return m_themeManager.get(); }

    ExternalUpdater* updater() { return m_updater.get(); }

    void triggerUpdateCheck();

    TranslationsModel* translations();

    JavaInstallList* javalist();

    InstanceList* instances() const { return m_instances.get(); }

    IconList* icons() const { return m_icons.get(); }

    MCEditTool* mcedit() const { return m_mcedit.get(); }

    AccountList* accounts() const { return m_accounts.get(); }

    Status status() const { return m_status; }

    const QMap<QString, std::shared_ptr<BaseProfilerFactory>>& profilers() const { return m_profilers; }

    void updateProxySettings(QString proxyTypeStr, QString addr, int port, QString user, QString password);

    QNetworkAccessManager* network();

    HttpMetaCache* metacache();

    Meta::Index* metadataIndex();

    void updateCapabilities();

    void detectLibraries();

    /*!
     * Finds and returns the full path to a jar file.
     * Returns a null-string if it could not be found.
     */
    QString getJarPath(QString jarFile);

    QString getMSAClientID();
    bool useMSALauncherLoginEndpoint();
    QString getFlameAPIKey();
    QString getModrinthAPIToken();
    QString getUserAgent();

    XylarSkinClient* skinClient() const { return m_skinClient; }
    void initSkinClient();

    /// this is the root of the 'installation'. Used for automatic updates
    const QString& root() { return m_rootPath; }

    /// the data path the application is using
    const QString& dataRoot() { return m_dataPath; }

    /// the java installed path the application is using
    const QString javaPath();

    bool isPortable() { return m_portable; }

    const Capabilities capabilities() { return m_capabilities; }

    /*!
     * Opens a json file using either a system default editor, or, if not empty, the editor
     * specified in the settings
     */
    bool openJsonEditor(const QString& filename);

    InstanceWindow* showInstanceWindow(BaseInstance* instance, QString page = QString());
    MainWindow* showMainWindow(bool minimized = false);
    ViewLogWindow* showLogWindow();

    void updateIsRunning(bool running);
    bool updatesAreAllowed();

    void ShowGlobalSettings(class QWidget* parent, QString open_page = QString());

    bool updaterEnabled();
    QString updaterBinaryName();

    QUrl normalizeImportUrl(const QString& url);

   signals:
    void updateAllowedChanged(bool status);
    void globalSettingsAboutToOpen();
    void globalSettingsApplied();

    void oauthReplyRecieved(QVariantMap);

#ifdef Q_OS_MACOS
    void clickedOnDock();
#endif

   public slots:
    /// Looks at the instance's last log and offers to fix what it finds (from the instance's menu).
    void repairInstance(BaseInstance* instance);
    bool launch(BaseInstance* instance,
                LaunchMode mode = LaunchMode::Normal,
                std::shared_ptr<MinecraftTarget> targetToJoin = nullptr,
                shared_qobject_ptr<MinecraftAccount> accountToUse = nullptr,
                const QString& offlineName = QString());
    bool kill(BaseInstance* instance);
    void closeCurrentWindow();

   private slots:
    void on_windowClose();
    void messageReceived(const QByteArray& message);
    void controllerFinished();
    /// After a failed start or a crash: says what went wrong and offers to fix it. @p crashedLater: the game
    /// was running a while, so it's only offered when the log shows a cause the repair knows.
    void offerAutoRepair(const QString& instanceId, LaunchMode mode, const QString& reason, bool crashedLater = false);
    void setupWizardFinished(int status);

   private:
    enum class RepairContext { FailedToStart, Crashed, Asked };
    void showRepairDialog(BaseInstance* instance, const InstanceRepair::Diagnosis& diagnosis, LaunchMode mode, const QString& reason,
                          RepairContext context);
    bool handleDataMigration(const QString& currentData, const QString& oldData, const QString& name, const QString& configFile) const;
    bool createSetupWizard();
    void performMainStartupAction();

    // sets the fatal error message and m_status to Failed.
    void showFatalErrorMessage(const QString& title, const QString& content);

   private:
    void addRunningInstance();
    void subRunningInstance();
    bool shouldExitNow() const;

   private:
    QHash<QString, int> m_qsaveResources;
    mutable QMutex m_qsaveResourcesMutex;

   private:
    QDateTime m_startTime;

    std::unique_ptr<QNetworkAccessManager> m_network;

    std::unique_ptr<ExternalUpdater> m_updater;
    std::unique_ptr<AccountList> m_accounts;

    std::unique_ptr<HttpMetaCache> m_metacache;
    std::unique_ptr<Meta::Index> m_metadataIndex;

    std::unique_ptr<SettingsObject> m_settings;
    std::unique_ptr<InstanceList> m_instances;
    std::unique_ptr<IconList> m_icons;
    std::unique_ptr<JavaInstallList> m_javalist;
    std::unique_ptr<TranslationsModel> m_translations;
    std::unique_ptr<GenericPageProvider> m_globalSettingsProvider;
    std::unique_ptr<MCEditTool> m_mcedit;
    QSet<QString> m_features;
    std::unique_ptr<ThemeManager> m_themeManager;
    XylarSkinClient* m_skinClient = nullptr;

    QMap<QString, std::shared_ptr<BaseProfilerFactory>> m_profilers;

    QString m_rootPath;
    QString m_dataPath;
    Status m_status = Application::StartingUp;
    Capabilities m_capabilities;
    bool m_portable = false;

#ifdef Q_OS_MACOS
    Qt::ApplicationState m_prevAppState = Qt::ApplicationInactive;
#endif

    // FIXME: attach to instances instead.
    struct InstanceXtras {
        InstanceWindow* window = nullptr;
        std::unique_ptr<LaunchController> controller;
    };
    std::map<QString, InstanceXtras> m_instanceExtras;
    mutable QMutex m_instanceExtrasMutex;
    /// Instances repaired after a failed start, with what was found, so the same problem isn't offered twice.
    QHash<QString, QString> m_repairedInstances;

    // main state variables
    size_t m_openWindows = 0;
    size_t m_runningInstances = 0;
    bool m_updateRunning = false;

    // main window, if any
    MainWindow* m_mainWindow = nullptr;

    // log window, if any
    ViewLogWindow* m_viewLogWindow = nullptr;

    // peer launcher instance connector - used to implement single instance launcher and signalling
    LocalPeer* m_peerInstance = nullptr;

    SetupWizard* m_setupWizard = nullptr;

   public:
    QString m_detectedGLFWPath;
    QString m_detectedOpenALPath;
    QString m_instanceIdToLaunch;
    QString m_serverToJoin;
    QString m_worldToJoin;
    QString m_profileToUse;
    bool m_launchOffline = false;
    QString m_offlineName;
    bool m_liveCheck = false;
    QList<QUrl> m_urlsToImport;
    QString m_instanceIdToShowWindowOf;
    bool m_showMainWindow = false;
    std::unique_ptr<QFile> logFile;
    std::unique_ptr<LogModel> logModel;

   public:
    void addQSavePath(QString);
    void removeQSavePath(QString);
    bool checkQSavePath(QString);
};
