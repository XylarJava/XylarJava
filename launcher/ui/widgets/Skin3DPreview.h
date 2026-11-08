#pragma once

#include <QImage>
#include <QPointF>
#include <QWidget>

/** Interactive, textured 3D preview using Qt's raster painter. No GPU or OpenGL dependency. */
class Skin3DPreview : public QWidget {
    Q_OBJECT
   public:
    explicit Skin3DPreview(QWidget* parent = nullptr);
    void setSkin(const QImage& skin, bool slim);
    void setCape(const QImage& cape);
    QSize sizeHint() const override { return QSize(240, 360); }

   protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;

   private:
    QImage m_skin, m_cape;
    bool m_slim = false;
    bool m_dragging = false;
    QPointF m_last;
    float m_yaw = -25, m_pitch = -12;
    qreal m_zoom = 1;
};
