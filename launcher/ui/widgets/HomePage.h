#pragma once

#include <QIcon>
#include <QImage>
#include <QList>
#include <QWidget>

class QLabel;
class QMenu;
class QPushButton;
class QToolButton;
class QFrame;
class QVBoxLayout;
class SkinPreview;
class HeroScene;

/**
 * The launcher's Home screen, laid out as a dashboard:
 *  - a greeting with the player's name,
 *  - a large "hero" card with an animated pixel-art scene, the player's skin and the
 *    selected instance with a big Play button,
 *  - an account card and an updates card on the right,
 *  - "Recently played" instances with their own Play / Stop buttons.
 *
 * All colours come from the active palette, so every built-in and community theme works.
 */
class HomePage : public QWidget {
    Q_OBJECT

   public:
    struct RecentInstance {
        QString id;
        QIcon icon;
        QString name;
        QString details;  // e.g. "Fabric 1.21.1  ·  2 hours ago"
        bool running = false;
        bool canLaunch = true;
    };

    explicit HomePage(QWidget* parent = nullptr);

    void setLogo(const QIcon& logo);

    /// Show the selected instance in the hero card.
    void showInstance(const QIcon& icon, const QString& name, const QString& details, bool canLaunch, bool running);

    /// Show the empty state. If other instances exist the user is asked to pick one, otherwise to create one.
    void showNoInstance(bool instancesExist);

    /// Show who will be playing and the skin they will have in game. A null skin shows a neutral silhouette.
    void showPlayer(const QString& name, const QString& note, const QImage& skin, bool slim);

    /// No account selected.
    void showNoPlayer();

    /// Up to a few instances, most recently played first.
    void setRecentInstances(const QList<RecentInstance>& instances);

    /// Version line and status text for the updates card.
    void setVersionInfo(const QString& version, const QString& status);

    /// Numbers for the stats card: how many instances, total time played, and the most played one.
    void setStats(int instanceCount, qint64 secondsPlayed, const QString& mostPlayed);

    /// Menu listing the instances; shown by the instance picker button.
    void setInstanceMenu(QMenu* menu);

    /// Optional menu with extra launch options (e.g. "Launch offline"), shown by the arrow next to Play.
    void setLaunchMenu(QMenu* menu);

    /// Accent colour for a palette: the highlight colour, or a readable fallback when a
    /// theme's highlight is too close to its window colour.
    static QColor accentFor(const QPalette& palette);
    /// Black or white, whichever reads better on the given background.
    static QColor textOn(const QColor& background);

   signals:
    void playRequested();
    void stopRequested();
    void manageInstancesRequested();
    void createInstanceRequested();
    void manageAccountsRequested();
    void checkUpdatesRequested();
    void playInstanceRequested(const QString& id);
    void stopInstanceRequested(const QString& id);
    void editInstanceRequested(const QString& id);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

   private:
    void updateStyle();
    void retranslate();
    void rebuildRecent();
    QFrame* makeCard(const QString& objectName);

    // Header
    QLabel* m_greetingFace = nullptr;
    QLabel* m_greeting = nullptr;
    QLabel* m_greetingSub = nullptr;

    // Hero
    HeroScene* m_hero = nullptr;
    SkinPreview* m_skin = nullptr;
    QLabel* m_nameplate = nullptr;
    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_detailsLabel = nullptr;
    QToolButton* m_pickerButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QToolButton* m_playMenuButton = nullptr;

    // Account card
    QLabel* m_accountFace = nullptr;
    QLabel* m_accountName = nullptr;
    QLabel* m_accountNote = nullptr;
    QPushButton* m_skinButton = nullptr;
    QPushButton* m_accountsButton = nullptr;

    // Updates card
    QLabel* m_logoLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QLabel* m_versionStatus = nullptr;
    QPushButton* m_updatesButton = nullptr;

    // Stats card
    QLabel* m_statsTitle = nullptr;
    QLabel* m_statInstances = nullptr;
    QLabel* m_statInstancesCaption = nullptr;
    QLabel* m_statPlaytime = nullptr;
    QLabel* m_statPlaytimeCaption = nullptr;
    QLabel* m_statFavourite = nullptr;
    int m_instanceCount = 0;
    qint64 m_secondsPlayed = 0;
    QString m_mostPlayed;

    // Recently played
    QLabel* m_recentTitle = nullptr;
    QVBoxLayout* m_recentLayout = nullptr;
    QLabel* m_recentEmpty = nullptr;
    QPushButton* m_manageButton = nullptr;
    QPushButton* m_addButton = nullptr;
    QList<RecentInstance> m_recent;

    bool m_hasInstance = false;
    bool m_instancesExist = false;
    bool m_running = false;
    bool m_hasPlayer = false;
    bool m_updatingStyle = false;
    bool m_restylePending = false;
    QString m_appliedStyleSheet;
};
