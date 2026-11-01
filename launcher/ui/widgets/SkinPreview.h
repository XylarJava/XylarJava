#pragma once

#include <QElapsedTimer>
#include <QImage>
#include <QWidget>

class QTimer;
class QPainter;

/**
 * Front view of a player's skin with a small idle animation (breathing and a
 * gentle arm sway). The texture is whatever the account actually uses, so the
 * preview matches what other players see in game.
 *
 * Without a texture it draws a neutral, theme-coloured silhouette instead.
 */
class SkinPreview : public QWidget {
    Q_OBJECT

   public:
    explicit SkinPreview(QWidget* parent = nullptr);

    /// A null image shows the placeholder silhouette.
    void setSkin(const QImage& texture, bool slim);
    bool hasSkin() const { return !m_skin.isNull(); }

    /// Guess the arm model from the texture (slim skins leave a transparent strip next to the arms).
    static bool looksSlim(const QImage& texture);

    QSize sizeHint() const override { return QSize(170, 290); }
    QSize minimumSizeHint() const override { return QSize(110, 190); }

   protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;

   private:
    void paintSkin(QPainter& painter, const QRectF& area, qreal phase);
    void paintPlaceholder(QPainter& painter, const QRectF& area, qreal phase);
    void paintPart(QPainter& painter, const QRect& source, const QRectF& target, bool mirror = false);

    QImage m_skin;
    bool m_slim = false;
    QTimer* m_timer = nullptr;
    QElapsedTimer m_clock;
};
