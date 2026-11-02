#pragma once
#include <java/JavaVersion.h>
#include <minecraft/mod/DataPackFolderModel.h>
#include <QDir>
#include <QProcess>
#include "BaseInstance.h"
#include "minecraft/launch/MinecraftTarget.h"
#include "minecraft/mod/Mod.h"

class ModFolderModel;
class ResourceFolderModel;
class ResourcePackFolderModel;
class ShaderPackFolderModel;
class TexturePackFolderModel;
class WorldList;
class LaunchStep;
class LaunchProfile;
class PackProfile;

class MinecraftInstance : public BaseInstance {
    Q_OBJECT
   public:
    MinecraftInstance(SettingsObject* globalSettings, std::unique_ptr<SettingsObject> settings, const QString& rootDir);
    virtual ~MinecraftInstance();
    virtual void saveNow() override;

    void loadSpecificSettings() override;

    // FIXME: remove
    QString typeName() const override;
    // FIXME: remove
    QSet<QString> traits() const override;

    bool canEdit() const override { return true; }

    bool canExport() const override { return true; }

    void populateLaunchMenu(QMenu* menu) override;

    ////// Directories and files //////
    QString jarModsDir() const;
    QString resourcePacksDir() const;
    QString texturePacksDir() const;
    QString shaderPacksDir() const;
    QString modsRoot() const override;
    QString coreModsDir() const;
    QString nilModsDir() const;
    QString dataPacksDir();
    QString modsCacheLocation() const;
    QString libDir() const;
    QString worldDir() const;
    QString resourcesDir() const;
    QDir jarmodsPath() const;
    QDir librariesPath() const;
    QDir versionsPath() const;
    QString instanceConfigFolder() const override;

    // Path to the instance's minecraft directory.
    QString gameRoot() const override;

    // Path to the instance's minecraft bin directory.
    QString binRoot() const;

    // where to put the natives during/before launch
    QString getNativePath() const;

    // where the instance-local libraries should be
    QString getLocalLibraryPath() const;

    /** Returns whether the instance, with its version, has support for demo mode. */
    bool supportsDemo() const;

    void updateRuntimeContext() override;

    //////  Profile management //////
    PackProfile* getPackProfile() const;

    //////  Mod Lists  //////
    ModFolderModel* loaderModList();
    ModFolderModel* coreModList();
    ModFolderModel* nilModList();
    ResourcePackFolderModel* resourcePackList();
    TexturePackFolderModel* texturePackList();
    ShaderPackFolderModel* shaderPackList();
    DataPackFolderModel* dataPackList();
    QList<ResourceFolderModel*> resourceLists();
    WorldList* worldList();

    //////  Launch stuff //////
    QList<Task::Ptr> createUpdateTask() override;
    LaunchTask* createLaunchTask(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin) override;
    QStringList extraArguments() override;
    QStringList verboseDescription(AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin) override;
    QList<Mod*> getJarMods() const;
    QString createLaunchScript(AuthSessionPtr session, MinecraftTarget::Ptr targetToJoin);
    /// get arguments passed to java
    /// JVM arguments for a launch. With the session, Microsoft accounts playing online keep the real
    /// Mojang services and everyone else gets XylarSkins.
    QStringList javaArguments(const AuthSession* session = nullptr);
    QString getLauncher();
    bool shouldApplyOnlineFixes();

    /// get variables for launch command variable substitution/environment
    QMap<QString, QString> getVariables() override;

    /// create an environment for launching processes
    QProcessEnvironment createEnvironment() override;
    QProcessEnvironment createLaunchEnvironment() override;

    QStringList getLogFileSearchPaths() override;

    QString getStatusbarDescription() override;

    // FIXME: remove
    virtual QStringList getClassPath();
    // FIXME: remove
    virtual QStringList getNativeJars();
    // FIXME: remove
    virtual QString getMainClass() const;

    // FIXME: remove
    virtual QStringList processMinecraftArgs(AuthSessionPtr account, MinecraftTarget::Ptr targetToJoin) const;

    virtual JavaVersion getJavaVersion();

   protected:
    QMap<QString, QString> createCensorFilterFromSession(AuthSessionPtr session);
    QMap<QString, QString> makeProfileVarMapping(std::shared_ptr<LaunchProfile> profile) const;

   protected:  // data
    std::unique_ptr<PackProfile> m_components;
    std::unique_ptr<ModFolderModel> m_loader_mod_list;
    std::unique_ptr<ModFolderModel> m_core_mod_list;
    std::unique_ptr<ModFolderModel> m_nil_mod_list;
    std::unique_ptr<ResourcePackFolderModel> m_resource_pack_list;
    std::unique_ptr<ShaderPackFolderModel> m_shader_pack_list;
    std::unique_ptr<TexturePackFolderModel> m_texture_pack_list;
    std::unique_ptr<DataPackFolderModel> m_data_pack_list;
    std::unique_ptr<WorldList> m_world_list;
};
