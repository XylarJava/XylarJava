#pragma once

#include <QElapsedTimer>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QWidget>

class QLabel;
class QMenu;
class QPushButton;
class QTimer;
class QToolButton;
class QFrame;
class QPainter;
class SkinPreview;

/**
 * The launcher's Home screen: a full-bleed pixel-art landscape (twinkling
 * stars, drifting clouds and a gentle mouse parallax), the player's skin as
 * it appears in game, and a launch card with the selected instance and a big
 * Play button.
 *
 * All colours are derived from the active palette, so the page follows
 * whichever built-in or community theme is selected.
 */
class HomePage : public QWidget {
    Q_OBJECT

   public:
    explicit HomePage(QWidget* parent = nullptr);

    void setLogo(const QIcon& logo);

    /// Show the selected instance in the launch card.
    void showInstance(const QIcon& icon, const QString& name, const QString& details, bool canLaunch, bool running);

    /// Show the empty state. If other instances exist the user is asked to pick one, otherwise to create one.
    void showNoInstance(bool instancesExist);

    /// Show who will be playing and the skin they will have in game. A null skin shows a neutral silhouette.
    void showPlayer(const QString& name, const QString& note, const QImage& skin, bool slim);

    /// No account selected.
    void showNoPlayer();

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

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

   private:
    void tick();
    void updateCardStyle();
    void retranslate();
    void paintBackdrop(QPainter& painter);
    void paintLiveLayer(QPainter& painter);

    QFrame* m_card = nullptr;
    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_detailsLabel = nullptr;
    QToolButton* m_pickerButton = nullptr;
    QPushButton* m_manageButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QToolButton* m_playMenuButton = nullptr;
    QLabel* m_logoLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    QFrame* m_playerPanel = nullptr;
    SkinPreview* m_skin = nullptr;
    QLabel* m_playerName = nullptr;
    QLabel* m_playerNote = nullptr;
    QPushButton* m_skinButton = nullptr;
    bool m_hasPlayer = false;

    QTimer* m_animTimer = nullptr;
    QElapsedTimer m_clock;
    QPointF m_parallax;        // current offset, -1..1 on both axes
    QPointF m_parallaxTarget;  // where the mouse wants it

    // The sky, sun and hills only change with size, palette and parallax: cache them.
    QPixmap m_backdrop;
    QPointF m_backdropParallax;
    bool m_backdropDirty = true;

    bool m_hasInstance = false;
    bool m_instancesExist = false;
    bool m_running = false;
    bool m_updatingStyle = false;
    QString m_appliedStyleSheet;
    quint32 m_seed = 0x5EED1234u;
};
