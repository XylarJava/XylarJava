#pragma once

#include <QFrame>
#include <QHash>
#include <QImage>
#include <QSet>
#include <QToolButton>

class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QPropertyAnimation;
class QPushButton;
class QVBoxLayout;
class MojangSkinFetcher;
class SocialClient;

/**
 * Friends panel on the right of the main window: your own row, an "add friend" field, friend
 * requests, then friends grouped into Online, Away and Offline (each group folds away).
 * Every avatar is the face of that player's skin: XylarSkins first, then Mojang, then the
 * game's default skin for the name.
 */
class SocialPanel : public QFrame {
    Q_OBJECT

   public:
    SocialPanel(SocialClient* client, QNetworkAccessManager* network, QWidget* parent = nullptr);

    /// Slides the panel in or out.
    void setOpen(bool open, bool animate = true);
    bool isOpen() const { return m_open; }

    /// 8x8 face plus hat layer of a skin texture, scaled with hard pixels to @p size.
    static QPixmap faceFrom(const QImage& skin, int size, qreal devicePixelRatio);

    /// Fetch this player's skin again now (e.g. after they changed it here).
    void refreshSkin(const QString& name);

    /// The skin found for @p name so far (null when none is loaded yet); skinChanged() tells when it changes.
    QImage skinFor(const QString& name) const { return m_skins.value(name.toLower()); }

    /// The server you host right now, by name, so friends can be invited to it; empty: none runs.
    void setInviteTarget(const QString& serverTitle);

   signals:
    void closeRequested();
    /// Join a friend's server (from an invite or from a friend who's playing there).
    void joinRequested(const QString& server, const QString& version, const QString& friendName);
    /// Invite this friend to the server set with setInviteTarget().
    void inviteRequested(const QString& name);
    /// Show this player's profile (yours or a friend's).
    void profileRequested(const QString& name);
    void skinChanged(const QString& name, const QImage& skin);

   protected:
    void changeEvent(QEvent* event) override;

   private:
    void rebuild();
    void updateStyle();
    void addFriend();
    void showNote(const QString& text, bool bad);
    QWidget* makeSection(const QString& key, const QString& title, int count);
    QWidget* makePersonRow(const QString& name, const QString& state, const QString& status, bool self,
                           const QString& server = QString(), const QString& version = QString());
    QWidget* makeRequestRow(const QString& name, bool incoming);
    QWidget* makeInviteRow(const QString& name, const QString& server, const QString& version, const QString& title);
    QPixmap avatarFor(const QString& name, const QString& state, int size);
    void loadSkin(const QString& name);
    void skinLoaded(const QString& name, const QImage& skin);

    SocialClient* m_client = nullptr;
    QNetworkAccessManager* m_network = nullptr;
    MojangSkinFetcher* m_mojang = nullptr;

    QWidget* m_body = nullptr;
    QVBoxLayout* m_list = nullptr;
    QLineEdit* m_addField = nullptr;
    QPushButton* m_addButton = nullptr;
    QLabel* m_note = nullptr;
    QPropertyAnimation* m_slide = nullptr;
    bool m_open = false;

    QSet<QString> m_collapsed;
    QHash<QString, QImage> m_skins;  ///< lowercase name -> skin texture (null image: none found)
    QHash<QString, qint64> m_skinTimes;  ///< lowercase name -> when its skin was fetched (ms since epoch)
    QSet<QString> m_loading;
    bool m_rebuildPending = false;
    QString m_appliedStyleSheet;
    QString m_inviteTitle;
};

/**
 * Round button that opens the Social panel, pinned to the top-right corner of @p host
 * (it follows the host as it resizes). A badge shows how many friend requests are waiting.
 */
class SocialButton : public QToolButton {
    Q_OBJECT

   public:
    explicit SocialButton(QWidget* host);

    void setBadge(int count);

   protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

   private:
    void place();

    int m_badge = 0;
    bool m_hovered = false;
};
