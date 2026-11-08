#include "Skin3DPreview.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTransform>
#include <QVector3D>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

Skin3DPreview::Skin3DPreview(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(160, 240);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("Drag to rotate · Scroll to zoom · Double-click to reset"));
    setAccessibleName(tr("Interactive 3D skin preview"));
}

void Skin3DPreview::setSkin(const QImage& skin, bool slim)
{
    m_skin = skin;
    m_slim = slim;
    update();
}

void Skin3DPreview::setCape(const QImage& cape)
{
    m_cape = cape;
    update();
}

void Skin3DPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    if (m_skin.isNull()) {
        painter.drawText(rect(), Qt::AlignCenter, tr("No skin available"));
        return;
    }
    struct Face {
        QPolygonF quad;
        QImage texture;
        float depth;
        int shade;
    };
    std::vector<Face> faces;
    const float yaw = qDegreesToRadians(m_yaw), pitch = qDegreesToRadians(m_pitch);
    auto rotate = [&](QVector3D v) {
        const float x = v.x() * qCos(yaw) + v.z() * qSin(yaw);
        const float z = -v.x() * qSin(yaw) + v.z() * qCos(yaw);
        return QVector3D(x, v.y() * qCos(pitch) - z * qSin(pitch), v.y() * qSin(pitch) + z * qCos(pitch));
    };
    const qreal scale = qMin(width() / 27.0, height() / 40.0) * m_zoom;
    auto project = [&](const QVector3D& v) {
        const qreal perspective = 96.0 / (96.0 - v.z());
        return QPointF(width() / 2.0 + v.x() * scale * perspective, height() / 2.0 - v.y() * scale * perspective);
    };
    // Minecraft's unfolded cube texture: top/bottom followed by right/front/left/back.
    auto box = [&](float cx, float top, float cz, int w, int h, int d, int u, int v, const QImage& texture, float inflate = 0,
                   bool capeTexture = false) {
        const float x0 = cx - w / 2.f - inflate, x1 = cx + w / 2.f + inflate;
        const float y1 = 16 - top + inflate, y0 = 16 - top - h - inflate;
        const float z0 = cz - d / 2.f - inflate, z1 = cz + d / 2.f + inflate;
        const std::array<QVector3D, 8> vertices = { QVector3D(x0, y1, z1), QVector3D(x1, y1, z1), QVector3D(x1, y0, z1),
                                                    QVector3D(x0, y0, z1), QVector3D(x1, y1, z0), QVector3D(x0, y1, z0),
                                                    QVector3D(x0, y0, z0), QVector3D(x1, y0, z0) };
        const int ids[6][4] = { { 0, 1, 2, 3 }, { 4, 5, 6, 7 }, { 5, 0, 3, 6 }, { 1, 4, 7, 2 }, { 5, 4, 1, 0 }, { 3, 2, 7, 6 } };
        const QVector3D normals[6] = { { 0, 0, 1 }, { 0, 0, -1 }, { -1, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 } };
        QRect uv[6] = { { u + d, v + d, w, h }, { u + 2 * d + w, v + d, w, h }, { u, v + d, d, h }, { u + d + w, v + d, d, h },
                        { u + d, v, w, d },     { u + d + w, v, w, d } };
        if (capeTexture)
            std::swap(uv[0], uv[1]);
        const int textureScale = qMax(1, texture.width() / 64);
        for (int i = 0; i < 6; ++i) {
            const QVector3D normal = rotate(normals[i]);
            QVector3D center;
            std::array<QVector3D, 4> world;
            for (int j = 0; j < 4; ++j) {
                world[j] = rotate(vertices[ids[i][j]]);
                center += world[j] / 4;
            }
            if (QVector3D::dotProduct(normal, QVector3D(0, 0, 96) - center) <= 0)
                continue;
            const QRect source(uv[i].topLeft() * textureScale, uv[i].size() * textureScale);
            if (!texture.rect().contains(source))
                continue;
            Face face;
            for (const auto& point : world)
                face.quad << project(point);
            face.texture = texture.copy(source);
            face.depth = center.z();
            face.shade = qBound(0, int(60 - normal.z() * 38 - normal.y() * 18), 95);
            {
                QPainter shade(&face.texture);
                shade.setCompositionMode(QPainter::CompositionMode_SourceAtop);
                shade.fillRect(face.texture.rect(), QColor(0, 0, 0, face.shade));
            }
            faces.push_back(std::move(face));
        }
    };
    const bool modern = m_skin.height() == m_skin.width();
    const int arm = m_slim ? 3 : 4;
    box(0, 0, 0, 8, 8, 8, 0, 0, m_skin);
    box(0, 8, 0, 8, 12, 4, 16, 16, m_skin);
    box(-4 - arm / 2.f, 8, 0, arm, 12, 4, 40, 16, m_skin);
    box(4 + arm / 2.f, 8, 0, arm, 12, 4, modern ? 32 : 40, modern ? 48 : 16, m_skin);
    box(-2, 20, 0, 4, 12, 4, 0, 16, m_skin);
    box(2, 20, 0, 4, 12, 4, modern ? 16 : 0, modern ? 48 : 16, m_skin);
    box(0, 0, 0, 8, 8, 8, 32, 0, m_skin, 0.5f);
    if (modern) {
        box(0, 8, 0, 8, 12, 4, 16, 32, m_skin, 0.25f);
        box(-4 - arm / 2.f, 8, 0, arm, 12, 4, 40, 32, m_skin, 0.25f);
        box(4 + arm / 2.f, 8, 0, arm, 12, 4, 48, 48, m_skin, 0.25f);
        box(-2, 20, 0, 4, 12, 4, 0, 32, m_skin, 0.25f);
        box(2, 20, 0, 4, 12, 4, 0, 48, m_skin, 0.25f);
    }
    if (!m_cape.isNull())
        box(0, 8, -3.5f, 10, 16, 1, 0, 0, m_cape, 0, true);
    std::stable_sort(faces.begin(), faces.end(), [](const Face& a, const Face& b) { return a.depth < b.depth; });
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    for (const auto& face : faces) {
        const QPolygonF source(QRectF(0, 0, face.texture.width(), face.texture.height()));
        QTransform transform;
        if (!QTransform::quadToQuad(source.mid(0, 4), face.quad, transform))
            continue;
        painter.save();
        painter.setTransform(transform);
        painter.drawImage(QPoint(0, 0), face.texture);
        // SourceAtop keeps transparent outer layers transparent when shading.
        painter.restore();
        // Texture alpha must not darken faces behind an empty hat/coat layer.
    }
}

void Skin3DPreview::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
        return;
    m_dragging = true;
    m_last = event->position();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
}
void Skin3DPreview::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging)
        return;
    const QPointF delta = event->position() - m_last;
    m_last = event->position();
    m_yaw = std::remainder(m_yaw + float(delta.x() * 0.6), 360.f);
    m_pitch = qBound(-85.f, m_pitch + float(delta.y() * 0.5), 85.f);
    update();
    event->accept();
}
void Skin3DPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
    }
}
void Skin3DPreview::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_yaw = -25;
        m_pitch = -12;
        m_zoom = 1;
        update();
        event->accept();
    }
}
void Skin3DPreview::wheelEvent(QWheelEvent* event)
{
    m_zoom = qBound(0.6, m_zoom * qPow(1.001, event->angleDelta().y()), 1.7);
    update();
    event->accept();
}
