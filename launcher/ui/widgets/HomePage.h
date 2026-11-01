#pragma once

#include <QIcon>
#include <QPointF>
#include <QWidget>

class QLabel;
class QMenu;
class QPushButton;
class QTimer;
class QToolButton;
class QFrame;
class QPainter;

/**
 * The launcher's Home screen: a full-bleed, procedurally painted pixel-art
 * landscape with a gentle mouse parallax, plus a launch card that holds the
 * selected instance and a large Play button.
 *
 * All colours are derived from the active palette, so the page follows
 * whichever built-in or community theme is selected.
 */
class HomePage : public QWidget {
    Q_OBJECT

   public:
    explicit HomePage(QWidget* parent = nullptr);

    /// Show the selected instance in the launch card.
    void showInstance(const QIcon& icon, const QString& name, const QString& details, bool canLaunch, bool running);

    /// Show the empty state. If other instances exist the user is asked to pick one, otherwise to create one.
    void showNoInstance(bool instancesExist);

    /// Menu listing the instances; shown by the instance picker button.
    void setInstanceMenu(QMenu* menu);

    /// Optional menu with extra launch options (e.g. "Launch offline"), shown by the arrow next to Play.
    void setLaunchMenu(QMenu* menu);

   signals:
    void playRequested();
    void stopRequested();
    void manageInstancesRequested();
    void createInstanceRequested();

   protected:
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

   private:
    void stepParallax();
    void updateCardStyle();
    void retranslate();
    void paintScenery(QPainter& painter);

    QFrame* m_card = nullptr;
    QLabel* m_iconLabel = nullptr;
    QLabel* m_nameLabel = nullptr;
    QLabel* m_detailsLabel = nullptr;
    QToolButton* m_pickerButton = nullptr;
    QPushButton* m_manageButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QToolButton* m_playMenuButton = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_subtitleLabel = nullptr;

    QTimer* m_animTimer = nullptr;
    QPointF m_parallax;        // current offset, -1..1 on both axes
    QPointF m_parallaxTarget;  // where the mouse wants it

    bool m_hasInstance = false;
    bool m_instancesExist = false;
    bool m_running = false;
    bool m_updatingStyle = false;
    QString m_appliedStyleSheet;
    quint32 m_seed = 0x5EED1234u;
};
