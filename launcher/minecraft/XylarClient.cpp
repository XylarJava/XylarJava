#include "XylarClient.h"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>

#include "Application.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/Component.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/ProfileUtils.h"
#include "settings/INISettingsObject.h"

namespace XylarClient {

QString displayName()
{
    return QStringLiteral("XylarJava Client 1.8");
}

bool hasClientComponent(BaseInstance* instance)
{
    auto minecraft = dynamic_cast<MinecraftInstance*>(instance);
    if (!minecraft)
        return false;
    auto profile = minecraft->getPackProfile();
    return profile && !profile->getComponentVersion(QString::fromLatin1(ComponentUid)).isEmpty();
}

bool isClientInstance(BaseInstance* instance)
{
    if (!instance)
        return false;
    if (hasClientComponent(instance))
        return true;
    // Instances made by "Set up" before a client jar was available.
    return instance->name().startsWith(displayName(), Qt::CaseInsensitive);
}

QString findClientJar()
{
    const QString fileName = QString::fromLatin1(JarFileName);
    const QStringList candidates = {
        FS::PathCombine(APPLICATION->dataRoot(), QStringLiteral("xylarjava-client"), fileName),
        FS::PathCombine(QCoreApplication::applicationDirPath(), fileName),
    };
    for (const QString& candidate : candidates) {
        if (QFileInfo(candidate).isFile())
            return candidate;
    }
    return QString();
}

}  // namespace XylarClient

std::unique_ptr<MinecraftInstance> XylarClientCreationTask::createInstance()
{
    setStatus(tr("Setting up %1").arg(XylarClient::displayName()));

    auto inst = std::make_unique<MinecraftInstance>(
        m_globalSettings, std::make_unique<INISettingsObject>(FS::PathCombine(m_stagingPath, "instance.cfg")), m_stagingPath);
    SettingsObject::Lock lock(inst->settings());

    auto components = inst->getPackProfile();
    components->buildingFromScratch();
    components->setComponentVersion(QStringLiteral("net.minecraft"), QStringLiteral("1.8.9"), true);

    const QString jar = XylarClient::findClientJar();
    if (!jar.isEmpty()) {
        const QString libraries = FS::PathCombine(m_stagingPath, QStringLiteral("libraries"));
        const QString patches = FS::PathCombine(m_stagingPath, QStringLiteral("patches"));
        const QString patchFile = FS::PathCombine(patches, QString::fromLatin1(XylarClient::ComponentUid) + QStringLiteral(".json"));
        bool ok = FS::ensureFolderPathExists(libraries) && FS::ensureFolderPathExists(patches) &&
                  QFile::copy(jar, FS::PathCombine(libraries, QString::fromLatin1(XylarClient::JarFileName)));

        // Written by hand rather than copied: files copied out of the resources are read-only.
        QFile source(QStringLiteral(":/xylar/io.xylarjava.client.json"));
        QFile target(patchFile);
        ok = ok && source.open(QIODevice::ReadOnly) && target.open(QIODevice::WriteOnly);
        if (ok) {
            ok = target.write(source.readAll()) > 0;
            target.close();
        }

        auto versionFile = ok ? ProfileUtils::parseJsonFile(QFileInfo(patchFile), false) : nullptr;
        if (versionFile) {
            components->appendComponent(makeShared<Component>(components, versionFile->uid, versionFile));
        } else {
            qWarning() << "Could not install the XylarJava Client component, the instance stays plain 1.8.9";
        }
    } else {
        qDebug() << "No XylarJava Client jar found, creating a plain 1.8.9 instance";
    }

    inst->setName(name());
    inst->setIconKey(m_instIcon);
    return inst;
}
