#include "FadingStackedWidget.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPropertyAnimation>

FadingStackedWidget::FadingStackedWidget(QWidget* parent) : QStackedWidget(parent) {}

void FadingStackedWidget::setCurrentIndexAnimated(int index)
{
    if (index == currentIndex() || index < 0 || index >= count())
        return;

    // Without a visible window there is nothing to fade.
    if (!isVisible() || width() <= 0 || height() <= 0 || m_duration <= 0) {
        setCurrentIndex(index);
        return;
    }

    // Snapshot the outgoing page, switch underneath it, then fade the snapshot away.
    const QPixmap snapshot = grab();
    setCurrentIndex(index);

    auto overlay = new QLabel(this);
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlay->setPixmap(snapshot);
    overlay->setGeometry(rect());
    overlay->show();
    overlay->raise();

    auto effect = new QGraphicsOpacityEffect(overlay);
    effect->setOpacity(1.0);
    overlay->setGraphicsEffect(effect);

    auto fade = new QPropertyAnimation(effect, "opacity", overlay);
    fade->setDuration(m_duration);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setEasingCurve(QEasingCurve::OutCubic);
    connect(fade, &QPropertyAnimation::finished, overlay, &QObject::deleteLater);
    fade->start();
}
