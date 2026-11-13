#include "Skin3DPreview.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QToolButton>
#include <QTransform>
#include <QVBoxLayout>
#include <QVector3D>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include "minecraft/skins/SkinModel.h"

Skin3DPreview::Skin3DPreview(QWidget* parent) : QWidget(parent)
{
    setMinimumSize(180, 270);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setCursor(Qt::OpenHandCursor);
    setToolTip(tr("Drag to rotate · Right-drag to move · Scroll to zoom · Double-click to reset\nArrow keys rotate; +/− zoom"));
    setAccessibleName(tr("Interactive 3D skin preview"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->addStretch();
    m_controls = new QWidget(this);
    m_controls->setObjectName(QStringLiteral("skinPreviewControls"));
    auto* buttons = new QHBoxLayout(m_controls);
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(4);
    buttons->addStretch();
    auto addButton = [&](const QString& text, const QString& name, auto action) {
        auto* button = new QToolButton(m_controls);
        button->setText(text);
        button->setObjectName(name);
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QToolButton::clicked, this, action);
        buttons->addWidget(button);
    };
    addButton(tr("Front"), QStringLiteral("skinFrontButton"), &Skin3DPreview::showFront);
    addButton(tr("Back"), QStringLiteral("skinBackButton"), &Skin3DPreview::showBack);
    addButton(tr("Reset"), QStringLiteral("skinResetButton"), &Skin3DPreview::resetView);
    buttons->addStretch();
    layout->addWidget(m_controls);
}

void Skin3DPreview::setSkin(const QImage& skin, bool slim)
{
    m_skin = SkinModel::normalizeTexture(skin);
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
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = QRectF(rect()).adjusted(8, 8, -8, -m_controls->height() - 18);
    const auto background = palette().color(QPalette::Base);
    QRadialGradient glow(area.center(), area.height() * 0.75);
    QColor light = palette().color(QPalette::Highlight);
    light.setAlpha(35);
    QColor edge = background;
    edge.setAlpha(165);
    glow.setColorAt(0, light);
    glow.setColorAt(1, edge);
    painter.setPen(Qt::NoPen);
    painter.setBrush(glow);
    painter.drawRoundedRect(area, 14, 14);
    if (m_skin.isNull()) {
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(area, Qt::AlignCenter, tr("No skin available"));
        return;
    }
    QPainterPath clip;
    clip.addRoundedRect(area, 14, 14);
    painter.setClipPath(clip);
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
    const qreal scale = qMin(area.width() / 25.0, area.height() / 38.0) * m_zoom;
    const auto center = area.center() + m_pan;
    auto project = [&](const QVector3D& v) {
        const qreal perspective = 96.0 / (96.0 - v.z());
        return QPointF(center.x() + v.x() * scale * perspective, center.y() - v.y() * scale * perspective);
    };
    const QPointF feet = project(rotate(QVector3D(0, -16, 0)));
    QRectF shadow(feet.x() - 8 * scale, feet.y() - 0.7 * scale, 16 * scale, 2.2 * scale);
    QRadialGradient floor(shadow.center(), shadow.width() / 2);
    floor.setColorAt(0, QColor(0, 0, 0, 75));
    floor.setColorAt(1, Qt::transparent);
    painter.setBrush(floor);
    painter.drawEllipse(shadow);
    const QVector3D keyLight = QVector3D(-0.4f, 0.8f, 1.f).normalized();
    // Minecraft's unfolded cube texture: top/bottom followed by right/front/left/back.
    auto box = [&](float cx, float top, float cz, int w, int h, int d, int u, int v, const QImage& texture, float inflate = 0,
                   bool capeTexture = false, float limbAngle = 0) {
        const float angle = qDegreesToRadians(limbAngle);
        auto pose = [&](QVector3D point) {
            const float x = point.x() - cx, y = point.y() - (16 - top);
            return QVector3D(cx + x * qCos(angle) - y * qSin(angle), 16 - top + x * qSin(angle) + y * qCos(angle), point.z());
        };
        auto poseNormal = [&](QVector3D normal) {
            return QVector3D(normal.x() * qCos(angle) - normal.y() * qSin(angle), normal.x() * qSin(angle) + normal.y() * qCos(angle),
                             normal.z());
        };
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
            const QVector3D normal = rotate(poseNormal(normals[i]));
            QVector3D center;
            std::array<QVector3D, 4> world;
            for (int j = 0; j < 4; ++j) {
                world[j] = rotate(pose(vertices[ids[i][j]]));
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
            face.shade = int(255 * (1.f - (0.64f + 0.36f * qMax(0.f, QVector3D::dotProduct(normal, keyLight)))));
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
    box(-4 - arm / 2.f, 8, 0, arm, 12, 4, 40, 16, m_skin, 0, false, -7);
    box(4 + arm / 2.f, 8, 0, arm, 12, 4, modern ? 32 : 40, modern ? 48 : 16, m_skin, 0, false, 7);
    box(-2, 20, 0, 4, 12, 4, 0, 16, m_skin);
    box(2, 20, 0, 4, 12, 4, modern ? 16 : 0, modern ? 48 : 16, m_skin);
    box(0, 0, 0, 8, 8, 8, 32, 0, m_skin, 0.5f);
    if (modern) {
        box(0, 8, 0, 8, 12, 4, 16, 32, m_skin, 0.25f);
        box(-4 - arm / 2.f, 8, 0, arm, 12, 4, 40, 32, m_skin, 0.25f, false, -7);
        box(4 + arm / 2.f, 8, 0, arm, 12, 4, 48, 48, m_skin, 0.25f, false, 7);
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
    if (event->button() != Qt::LeftButton && event->button() != Qt::RightButton)
        return;
    setFocus(Qt::MouseFocusReason);
    m_dragging = true;
    m_panning = event->button() == Qt::RightButton;
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
    if (m_panning) {
        m_pan += delta;
        m_pan.setX(qBound(-width() * 0.35, m_pan.x(), width() * 0.35));
        m_pan.setY(qBound(-height() * 0.35, m_pan.y(), height() * 0.35));
    } else {
        m_yaw = std::remainder(m_yaw + float(delta.x() * 0.45), 360.f);
        m_pitch = qBound(-85.f, m_pitch + float(delta.y() * 0.35), 85.f);
    }
    update();
    event->accept();
}
void Skin3DPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton || event->button() == Qt::RightButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
    }
}
void Skin3DPreview::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        resetView();
        event->accept();
    }
}
void Skin3DPreview::wheelEvent(QWheelEvent* event)
{
    const int delta = event->angleDelta().isNull() ? event->pixelDelta().y() : event->angleDelta().y();
    m_zoom = qBound(0.55, m_zoom * qPow(1.001, delta), 2.0);
    update();
    event->accept();
}

void Skin3DPreview::resetView()
{
    m_yaw = -25;
    m_pitch = -12;
    m_zoom = 1;
    m_pan = {};
    update();
}
void Skin3DPreview::showFront()
{
    m_yaw = 0;
    m_pitch = -8;
    m_pan = {};
    update();
}
void Skin3DPreview::showBack()
{
    m_yaw = 180;
    m_pitch = -8;
    m_pan = {};
    update();
}
void Skin3DPreview::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Left:
            m_yaw -= 10;
            break;
        case Qt::Key_Right:
            m_yaw += 10;
            break;
        case Qt::Key_Up:
            m_pitch = qMax(-85.f, m_pitch - 10);
            break;
        case Qt::Key_Down:
            m_pitch = qMin(85.f, m_pitch + 10);
            break;
        case Qt::Key_Plus:
        case Qt::Key_Equal:
            m_zoom = qMin(2.0, m_zoom * 1.1);
            break;
        case Qt::Key_Minus:
            m_zoom = qMax(0.55, m_zoom / 1.1);
            break;
        case Qt::Key_R:
            resetView();
            return;
        default:
            QWidget::keyPressEvent(event);
            return;
    }
    update();
    event->accept();
}
