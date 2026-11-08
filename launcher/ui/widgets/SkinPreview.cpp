#include "SkinPreview.h"

#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QtMath>

namespace {
QColor blend(const QColor& a, const QColor& b, qreal t)
{
    const float f = float(qBound<qreal>(0.0, t, 1.0));
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f,
                            a.blueF() + (b.blueF() - a.blueF()) * f, 1.0f);
}
}  // namespace

SkinPreview::SkinPreview(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("skinPreview"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_timer = new QTimer(this);
    m_timer->setInterval(33);
    connect(m_timer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
    m_clock.start();
}

bool SkinPreview::looksSlim(const QImage& texture)
{
    if (texture.width() != 64 || texture.height() != 64)
        return false;
    // Classic arms fill x 40..55 on rows 20..31; slim arms stop at x 53.
    return qAlpha(texture.pixel(55, 20)) == 0 && qAlpha(texture.pixel(55, 31)) == 0;
}

void SkinPreview::setSkin(const QImage& texture, bool slim)
{
    if (texture.isNull() || texture.width() < 64 || texture.width() % 64 ||
        (texture.height() != texture.width() && texture.height() * 2 != texture.width())) {
        m_skin = QImage();
    } else {
        m_skin = texture.scaledToWidth(64, Qt::FastTransformation).convertToFormat(QImage::Format_ARGB32);
    }
    m_slim = slim;
    update();
}

void SkinPreview::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_timer->start();
}

void SkinPreview::hideEvent(QHideEvent* event)
{
    m_timer->stop();
    QWidget::hideEvent(event);
}

void SkinPreview::changeEvent(QEvent* event)
{
    // Pause the idle animation while the launcher is in the background.
    if (event->type() == QEvent::ActivationChange) {
        if (isActiveWindow() && isVisible())
            m_timer->start();
        else
            m_timer->stop();
    }
    QWidget::changeEvent(event);
}

void SkinPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const qreal seconds = m_clock.elapsed() / 1000.0;
    // A slow, calm idle: one breath every ~8 seconds, the arms sway at half that pace.
    const qreal phase = seconds * 0.78;
    const QRectF area = QRectF(rect()).adjusted(8, 8, -8, -8);

    if (m_skin.isNull())
        paintPlaceholder(painter, area, phase);
    else
        paintSkin(painter, area, phase);
}

void SkinPreview::paintPart(QPainter& painter, const QRect& source, const QRectF& target, bool mirror)
{
    QImage part = m_skin.copy(source);
    if (mirror)
        part = part.mirrored(true, false);
    painter.drawImage(target, part);
}

void SkinPreview::paintSkin(QPainter& painter, const QRectF& area, qreal phase)
{
    const bool legacy = m_skin.height() == 32;
    const int armW = m_slim ? 3 : 4;

    // The model is (armW + 8 + armW) x 32 skin pixels; fit it into the area.
    const qreal modelW = armW * 2 + 8;
    const qreal unit = qMin(area.width() / modelW, (area.height() - 14) / 32.0);
    const qreal bob = qSin(phase) * unit * 0.22;
    const qreal swing = qSin(phase * 0.5) * 3.5;  // degrees

    const qreal left = area.center().x() - modelW * unit / 2.0;
    const qreal top = area.top() + (area.height() - 14 - 32 * unit) / 2.0;

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Soft shadow under the feet.
    {
        const QColor shadowColor = QApplication::palette(this).color(QPalette::Shadow);
        QColor shadow = shadowColor;
        shadow.setAlpha(70);
        const qreal shadowW = modelW * unit * (0.95 - qSin(phase) * 0.04);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadow);
        painter.drawEllipse(QPointF(area.center().x(), top + 32 * unit + 6), shadowW / 2.0, unit * 1.1);
    }

    auto rectAt = [&](qreal x, qreal y, qreal w, qreal h) { return QRectF(left + x * unit, top + y * unit, w * unit, h * unit); };

    // Legs (stay planted).
    paintPart(painter, QRect(4, 20, 4, 12), rectAt(armW, 20, 4, 12));
    if (legacy)
        paintPart(painter, QRect(4, 20, 4, 12), rectAt(armW + 4, 20, 4, 12), true);
    else
        paintPart(painter, QRect(20, 52, 4, 12), rectAt(armW + 4, 20, 4, 12));
    if (!legacy) {
        paintPart(painter, QRect(4, 36, 4, 12), rectAt(armW, 20, 4, 12));
        paintPart(painter, QRect(4, 52, 4, 12), rectAt(armW + 4, 20, 4, 12));
    }

    // Upper body breathes up and down a little.
    painter.save();
    painter.translate(0, bob);

    // Arms swing gently from the shoulders.
    auto drawArm = [&](qreal x, const QRect& base, const QRect& overlay, bool mirror, qreal angle) {
        painter.save();
        const QPointF shoulder(left + (x + armW / 2.0) * unit, top + 8 * unit);
        painter.translate(shoulder);
        painter.rotate(angle);
        painter.translate(-shoulder);
        paintPart(painter, base, rectAt(x, 8, armW, 12), mirror);
        if (!overlay.isNull())
            paintPart(painter, overlay, rectAt(x, 8, armW, 12), mirror);
        painter.restore();
    };
    drawArm(0, QRect(44, 20, armW, 12), legacy ? QRect() : QRect(44, 36, armW, 12), false, swing);
    if (legacy)
        drawArm(armW + 8, QRect(44, 20, armW, 12), QRect(), true, -swing);
    else
        drawArm(armW + 8, QRect(36, 52, armW, 12), QRect(52, 52, armW, 12), false, -swing);

    // Body and head.
    paintPart(painter, QRect(20, 20, 8, 12), rectAt(armW, 8, 8, 12));
    if (!legacy)
        paintPart(painter, QRect(20, 36, 8, 12), rectAt(armW, 8, 8, 12));
    paintPart(painter, QRect(8, 8, 8, 8), rectAt(armW, 0, 8, 8));
    // The hat layer sits slightly proud of the head, as it does in game.
    paintPart(painter, QRect(40, 8, 8, 8), rectAt(armW - 0.5, -0.5, 9, 9));

    painter.restore();
}

void SkinPreview::paintPlaceholder(QPainter& painter, const QRectF& area, qreal phase)
{
    // A neutral, rounded figure in theme colours: clearly "no skin yet".
    const QPalette pal = QApplication::palette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor body = blend(bg, fg, 0.22);
    const QColor head = blend(bg, fg, 0.30);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    const qreal h = qMin(area.height() - 14, area.width() * 1.9);
    const qreal w = h / 1.9;
    const qreal cx = area.center().x();
    const qreal top = area.top() + (area.height() - 14 - h) / 2.0;
    const qreal bob = qSin(phase) * h * 0.008;

    QColor shadow = pal.color(QPalette::Shadow);
    shadow.setAlpha(60);
    painter.setBrush(shadow);
    painter.drawEllipse(QPointF(cx, top + h + 6), w * 0.45, h * 0.025);

    painter.setBrush(body);
    QPainterPath torso;
    torso.addRoundedRect(QRectF(cx - w * 0.42, top + h * 0.36 + bob, w * 0.84, h * 0.64 - bob), w * 0.30, w * 0.30);
    painter.drawPath(torso);

    painter.setBrush(head);
    painter.drawEllipse(QPointF(cx, top + h * 0.18 + bob), w * 0.27, w * 0.27);
}
