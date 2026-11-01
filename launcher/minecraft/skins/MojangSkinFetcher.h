#pragma once

#include <QHash>
#include <QImage>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * Fetches a player's real skin from Mojang's public APIs:
 *  - by profile UUID (Microsoft accounts): sessionserver profile -> textures -> skin PNG
 *  - by username (offline accounts): api.mojang.com name lookup first, then as above
 *
 * Results (including "this player has no Mojang profile") are cached in memory
 * for the session and on disk, so the Home screen doesn't hit the network on
 * every repaint or account switch.
 */
class MojangSkinFetcher : public QObject {
    Q_OBJECT

   public:
    explicit MojangSkinFetcher(QNetworkAccessManager* network, QObject* parent = nullptr);

    struct Skin {
        QImage texture;
        bool slim = false;
    };

    /// Cached result for a key ("uuid:<id>" or "name:<lowercase name>"), if any.
    bool cached(const QString& key, Skin* out) const;

    static QString keyForUuid(const QString& uuid);
    static QString keyForName(const QString& name);

    void fetchByUuid(const QString& uuid);
    void fetchByName(const QString& name);

   signals:
    /// Emitted once per request. A null texture means the player has no Mojang skin.
    void finished(const QString& key, const QImage& texture, bool slim);

   private:
    void fetchProfile(const QString& key, const QString& uuid);
    void fetchTexture(const QString& key, const QString& url, bool slim);
    void done(const QString& key, const QImage& texture, bool slim);
    void failed(const QString& key);
    QString diskPath(const QString& key) const;
    bool loadFromDisk(const QString& key, Skin* out) const;

    QNetworkAccessManager* m_network = nullptr;
    QHash<QString, Skin> m_cache;
    QHash<QString, bool> m_pending;
};
