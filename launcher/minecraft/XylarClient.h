#pragma once

#include <QString>

#include "InstanceCreationTask.h"

class BaseInstance;

/// The XylarJava Client for Minecraft 1.8.9 (see client/ in the repository).
namespace XylarClient {

/// Component uid of the client in an instance's pack profile.
inline constexpr const char* ComponentUid = "io.xylarjava.client";
/// File name of the client jar in the instance's libraries folder.
inline constexpr const char* JarFileName = "xylarjava-client-0.1.0.jar";

/// Name shown in the launcher, whatever the instance is called.
QString displayName();

/// Whether this instance is (or is meant to be) the XylarJava Client instance.
bool isClientInstance(BaseInstance* instance);

/// Whether the client component is installed in the instance.
bool hasClientComponent(BaseInstance* instance);

/// A client jar the launcher can install, or an empty string if there is none.
/// Looked up in "<data folder>/xylarjava-client/" and next to the launcher executable.
QString findClientJar();

}  // namespace XylarClient

/// Creates a Minecraft 1.8.9 instance for the XylarJava Client. When a client jar is available the
/// client component is installed too; otherwise the instance starts out as plain 1.8.9.
class XylarClientCreationTask final : public InstanceCreationTask {
    Q_OBJECT
   public:
    XylarClientCreationTask() = default;

   protected:
    std::unique_ptr<MinecraftInstance> createInstance() override;
};
