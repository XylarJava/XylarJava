#include "DefaultSkin.h"

#include <QCollator>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QStringList>
#include <algorithm>

#include "archive/ArchiveReader.h"

namespace DefaultSkin {

namespace {

// Order used by the game since 1.19.3: the nine characters as slim models,
// then the same nine as classic ("wide") models.
// Minecraft 1.8.9's client jar, as listed in Mojang's version data.
constexpr auto kJarVersion = "1.8.9";
constexpr auto kJarSha1 = "3870888a6c3d349d3771a3e9d16c9bf5e076b908";
constexpr auto kJarUrl = "https://launcher.mojang.com/v1/objects/3870888a6c3d349d3771a3e9d16c9bf5e076b908/client.jar";

QList<std::pair<QPointer<QObject>, std::function<void(bool)>>> g_waiting;
bool g_downloading = false;

const char* const kModernNames[] = { "alex", "ari", "efe", "kai", "makena", "noor", "steve", "sunny", "zuri" };
constexpr int kModernCount = 9;

QString clientJarFor(const QDir& base, const QString& version)
{
    return base.absoluteFilePath(QStringLiteral("%1/minecraft-%1-client.jar").arg(version));
}

// Client jars the launcher has downloaded, preferred version first, then newest first.
QStringList candidateJars(const QString& preferredVersion)
{
    QStringList jars;
    const QDir base(QDir(QStringLiteral("libraries")).absoluteFilePath(QStringLiteral("com/mojang/minecraft")));
    if (!base.exists())
        return jars;

    if (!preferredVersion.isEmpty()) {
        const QString preferred = clientJarFor(base, preferredVersion);
        if (QFileInfo::exists(preferred))
            jars << preferred;
    }

    QStringList versions = base.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(versions.begin(), versions.end(), [&collator](const QString& a, const QString& b) { return collator.compare(a, b) > 0; });
    for (const QString& version : versions) {
        const QString jar = clientJarFor(base, version);
        if (QFileInfo::exists(jar) && !jars.contains(jar))
            jars << jar;
        if (jars.size() >= 4)
            break;
    }
    return jars;
}

QImage readTexture(const QString& jar, const QString& path)
{
    MMCZip::ArchiveReader reader(jar);
    auto file = reader.goToFile(path);
    if (!file)
        return QImage();
    QImage image;
    if (!image.loadFromData(file->readAll(), "PNG"))
        return QImage();
    return image;
}

bool parseHalf(const QString& hex, quint64& out)
{
    bool ok = false;
    out = hex.toULongLong(&ok, 16);
    return ok;
}

}  // namespace

int uuidHashCode(const QString& uuid)
{
    QString hex = uuid;
    hex.remove(QLatin1Char('-'));
    if (hex.length() != 32)
        return 0;

    quint64 most = 0;
    quint64 least = 0;
    if (!parseHalf(hex.left(16), most) || !parseHalf(hex.mid(16), least))
        return 0;

    const quint64 hilo = most ^ least;
    return static_cast<int>(static_cast<quint32>(hilo >> 32) ^ static_cast<quint32>(hilo & 0xFFFFFFFFu));
}

Result forUuid(const QString& uuid, const QString& preferredVersion)
{
    Result result;
    const int hash = uuidHashCode(uuid);

    // Modern selection (1.19.3+): floorMod(hash, 18).
    const int modernIndex = ((hash % (kModernCount * 2)) + kModernCount * 2) % (kModernCount * 2);
    const bool modernSlim = modernIndex < kModernCount;
    const QString modernName = QString::fromLatin1(kModernNames[modernIndex % kModernCount]);

    // Older versions only know Steve and Alex: odd hash means Alex.
    const bool legacySlim = (hash & 1) != 0;
    const QString legacyName = legacySlim ? QStringLiteral("alex") : QStringLiteral("steve");

    for (const QString& jar : candidateJars(preferredVersion)) {
        const QString modernPath = QStringLiteral("assets/minecraft/textures/entity/player/%1/%2.png")
                                       .arg(modernSlim ? QStringLiteral("slim") : QStringLiteral("wide"), modernName);
        QImage texture = readTexture(jar, modernPath);
        if (!texture.isNull()) {
            result.texture = texture;
            result.slim = modernSlim;
            result.name = modernName;
            return result;
        }

        texture = readTexture(jar, QStringLiteral("assets/minecraft/textures/entity/%1.png").arg(legacyName));
        if (!texture.isNull()) {
            result.texture = texture;
            result.slim = legacySlim;
            result.name = legacyName;
            return result;
        }
    }
    return result;
}

bool haveClientJar()
{
    return !candidateJars(QString()).isEmpty();
}

void downloadClientJar(QNetworkAccessManager* network, QObject* context, std::function<void(bool ok)> done)
{
    g_waiting.append({ QPointer<QObject>(context), std::move(done) });
    if (g_downloading)
        return;
    g_downloading = true;

    auto finish = [](bool ok) {
        g_downloading = false;
        const auto waiting = std::exchange(g_waiting, {});
        for (const auto& [target, callback] : waiting) {
            if (target && callback)
                callback(ok);
        }
    };

    QNetworkRequest request{ QUrl(QString::fromLatin1(kJarUrl)) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = network->get(request);
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply, finish] {
        reply->deleteLater();
        const QByteArray data = reply->error() == QNetworkReply::NoError ? reply->readAll() : QByteArray();
        if (data.isEmpty() || QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex() != kJarSha1) {
            qWarning() << "Couldn't download the Minecraft client jar for default skins:" << reply->errorString();
            finish(false);
            return;
        }
        const QDir base(QDir(QStringLiteral("libraries")).absoluteFilePath(QStringLiteral("com/mojang/minecraft")));
        const QString path = clientJarFor(base, QString::fromLatin1(kJarVersion));
        QSaveFile file(path);  // written whole or not at all, so the launcher never sees half a jar
        const bool ok = QDir().mkpath(QFileInfo(path).absolutePath()) && file.open(QIODevice::WriteOnly) && file.write(data) == data.size() &&
                        file.commit();
        if (!ok)
            qWarning() << "Couldn't save" << path << file.errorString();
        finish(ok);
    });
}

}  // namespace DefaultSkin
