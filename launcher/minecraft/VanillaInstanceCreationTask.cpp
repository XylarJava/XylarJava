#include "VanillaInstanceCreationTask.h"

#include <utility>

#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "settings/INISettingsObject.h"

VanillaCreationTask::VanillaCreationTask(BaseVersion::Ptr version, QString loader, BaseVersion::Ptr loader_version)
    : InstanceCreationTask()
    , m_version(std::move(version))
    , m_using_loader(true)
    , m_loader(std::move(loader))
    , m_loader_version(std::move(loader_version))
{}

std::unique_ptr<MinecraftInstance> VanillaCreationTask::createInstance()
{
    const QString versionId = m_version ? m_version->descriptor() : m_versionId;
    setStatus(tr("Creating instance from version %1").arg(m_version ? m_version->name() : m_versionId));

    auto inst = std::make_unique<MinecraftInstance>(m_globalSettings, std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")),
                           m_stagingPath);
    SettingsObject::Lock lock(inst->settings());
    inst->settings()->set("NativeRust", m_nativeRust);

    auto components = inst->getPackProfile();
    components->buildingFromScratch();
    components->setComponentVersion("net.minecraft", versionId, true);
    if (m_using_loader && !m_nativeRust)
        components->setComponentVersion(m_loader, m_loader_version->descriptor());

    inst->setName(name());
    inst->setIconKey(m_instIcon);

    return inst;
}
