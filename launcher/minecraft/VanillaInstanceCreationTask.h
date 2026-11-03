#pragma once

#include "InstanceCreationTask.h"

#include <utility>

class VanillaCreationTask final : public InstanceCreationTask {
    Q_OBJECT
   public:
    VanillaCreationTask(BaseVersion::Ptr version) : InstanceCreationTask(), m_version(std::move(version)) {}
    VanillaCreationTask(BaseVersion::Ptr version, QString loader, BaseVersion::Ptr loader_version);
    /// A plain Minecraft instance of this version ("1.21.4"), without looking the version list up first.
    explicit VanillaCreationTask(QString minecraftVersion) : InstanceCreationTask(), m_versionId(std::move(minecraftVersion)) {}

    std::unique_ptr<MinecraftInstance> createInstance() override;

   private:
    // Version to update to / create of the instance.
    BaseVersion::Ptr m_version;
    QString m_versionId;  ///< used when m_version is null

    bool m_using_loader = false;
    QString m_loader;
    BaseVersion::Ptr m_loader_version;
};
