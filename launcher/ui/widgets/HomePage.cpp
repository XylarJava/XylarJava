#include "HomePage.h"

#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

#include "BuildConfig.h"

namespace {

QColor mix(const QColor& a, const QColor& b, qreal t)
{
    const float f = float(qBound<qreal>(0.0, t, 1.0));
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f,
                            a.blueF() + (b.blueF() - a.blueF()) * f, 1.0f);
}

QString rgba(const QColor& c, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

// WCAG relative luminance and contrast ratio.
qreal luminance(const QColor& c)
{
    auto channel = [](qreal v) { return v <= 0.03928 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    return 0.2126 * channel(qreal(c.redF())) + 0.7152 * channel(qreal(c.greenF())) + 0.0722 * channel(qreal(c.blueF()));
}

qreal contrast(const QColor& a, const QColor& b)
{
    const qreal la = luminance(a);
    const qreal lb = luminance(b);
    return (qMax(la, lb) + 0.05) / (qMin(la, lb) + 0.05);
}

// Small deterministic PRNG so the scenery looks the same on every repaint.
quint32 nextRandom(quint32& state)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

qreal randomUnit(quint32& state)
{
    return (nextRandom(state) & 0xFFFFFFu) / qreal(0x1000000);
}

}  // namespace

HomePage::HomePage(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("homePage"));
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    m_animTimer = new QTimer(this);
    m_animTimer->setInterval(16);
    connect(m_animTimer, &QTimer::timeout, this, &HomePage::stepParallax);

    // --- Hero text -------------------------------------------------------
    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName(QStringLiteral("homeTitle"));
    m_titleLabel->setText(BuildConfig.LAUNCHER_DISPLAYNAME);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setObjectName(QStringLiteral("homeSubtitle"));
    m_subtitleLabel->setWordWrap(true);

    // --- Launch card -----------------------------------------------------
    m_card = new QFrame(this);
    m_card->setObjectName(QStringLiteral("launchCard"));
    m_card->setMouseTracking(true);

    m_iconLabel = new QLabel(m_card);
    m_iconLabel->setObjectName(QStringLiteral("launchCardIcon"));
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setAlignment(Qt::AlignCenter);

    m_nameLabel = new QLabel(m_card);
    m_nameLabel->setObjectName(QStringLiteral("launchCardName"));
    m_nameLabel->setTextFormat(Qt::PlainText);

    m_detailsLabel = new QLabel(m_card);
    m_detailsLabel->setObjectName(QStringLiteral("launchCardDetails"));
    m_detailsLabel->setTextFormat(Qt::PlainText);

    m_pickerButton = new QToolButton(m_card);
    m_pickerButton->setObjectName(QStringLiteral("launchCardPicker"));
    m_pickerButton->setPopupMode(QToolButton::InstantPopup);
    m_pickerButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_pickerButton->setCursor(Qt::PointingHandCursor);

    m_manageButton = new QPushButton(m_card);
    m_manageButton->setObjectName(QStringLiteral("launchCardManage"));
    m_manageButton->setFlat(true);
    m_manageButton->setCursor(Qt::PointingHandCursor);
    connect(m_manageButton, &QPushButton::clicked, this, &HomePage::manageInstancesRequested);

    m_playButton = new QPushButton(m_card);
    m_playButton->setObjectName(QStringLiteral("playButton"));
    m_playButton->setCursor(Qt::PointingHandCursor);
    m_playButton->setMinimumHeight(52);
    m_playButton->setMinimumWidth(170);
    connect(m_playButton, &QPushButton::clicked, this, [this] {
        if (!m_hasInstance)
            emit createInstanceRequested();
        else if (m_running)
            emit stopRequested();
        else
            emit playRequested();
    });

    m_playMenuButton = new QToolButton(m_card);
    m_playMenuButton->setObjectName(QStringLiteral("playMenuButton"));
    m_playMenuButton->setPopupMode(QToolButton::InstantPopup);
    m_playMenuButton->setArrowType(Qt::DownArrow);
    m_playMenuButton->setMinimumHeight(52);
    m_playMenuButton->setCursor(Qt::PointingHandCursor);
    m_playMenuButton->setVisible(false);

    auto nameRow = new QHBoxLayout();
    nameRow->setContentsMargins(0, 0, 0, 0);
    nameRow->setSpacing(6);
    nameRow->addWidget(m_nameLabel);
    nameRow->addWidget(m_pickerButton);
    nameRow->addStretch(1);

    auto textColumn = new QVBoxLayout();
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(2);
    textColumn->addLayout(nameRow);
    textColumn->addWidget(m_detailsLabel);
    textColumn->addWidget(m_manageButton, 0, Qt::AlignLeft);

    auto playRow = new QHBoxLayout();
    playRow->setContentsMargins(0, 0, 0, 0);
    playRow->setSpacing(0);
    playRow->addWidget(m_playButton);
    playRow->addWidget(m_playMenuButton);

    auto cardLayout = new QHBoxLayout(m_card);
    cardLayout->setContentsMargins(18, 16, 18, 16);
    cardLayout->setSpacing(14);
    cardLayout->addWidget(m_iconLabel, 0, Qt::AlignTop);
    cardLayout->addLayout(textColumn, 1);
    cardLayout->addLayout(playRow, 0);

    // --- Page layout -----------------------------------------------------
    auto pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(36, 32, 36, 28);
    pageLayout->setSpacing(6);
    pageLayout->addStretch(3);
    pageLayout->addWidget(m_titleLabel);
    pageLayout->addWidget(m_subtitleLabel);
    pageLayout->addStretch(2);
    pageLayout->addWidget(m_card);

    retranslate();
    updateCardStyle();
    showNoInstance(false);
}

void HomePage::retranslate()
{
    m_subtitleLabel->setText(tr("Pick an instance and jump straight into the game."));
    m_manageButton->setText(tr("Manage instances"));
    m_pickerButton->setText(tr("Switch"));
    m_pickerButton->setToolTip(tr("Choose which instance to play"));
    m_playMenuButton->setToolTip(tr("More launch options"));
}

void HomePage::showInstance(const QIcon& icon, const QString& name, const QString& details, bool canLaunch, bool running)
{
    m_hasInstance = true;
    m_instancesExist = true;
    m_running = running;

    m_iconLabel->setPixmap(icon.pixmap(40, 40));
    m_nameLabel->setText(name);
    m_detailsLabel->setText(details);
    m_pickerButton->setVisible(true);
    m_manageButton->setVisible(true);
    if (m_running) {
        m_playButton->setText(tr("STOP"));
        m_playButton->setEnabled(true);
    } else {
        m_playButton->setText(tr("PLAY"));
        m_playButton->setEnabled(canLaunch);
    }
    m_playMenuButton->setEnabled(!m_running && canLaunch);
}

void HomePage::showNoInstance(bool instancesExist)
{
    m_hasInstance = false;
    m_instancesExist = instancesExist;
    m_running = false;

    m_iconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("new")).pixmap(40, 40));
    m_pickerButton->setVisible(instancesExist);
    m_manageButton->setVisible(instancesExist);
    m_playMenuButton->setEnabled(false);
    if (instancesExist) {
        m_nameLabel->setText(tr("No instance selected"));
        m_detailsLabel->setText(tr("Use \"Switch\" to choose what to play."));
        m_playButton->setText(tr("PLAY"));
        m_playButton->setEnabled(false);
    } else {
        m_nameLabel->setText(tr("No instances yet"));
        m_detailsLabel->setText(tr("Create your first instance to start playing."));
        m_playButton->setText(tr("CREATE"));
        m_playButton->setEnabled(true);
    }
}

void HomePage::setInstanceMenu(QMenu* menu)
{
    m_pickerButton->setMenu(menu);
}

void HomePage::setLaunchMenu(QMenu* menu)
{
    m_playMenuButton->setMenu(menu);
    m_playMenuButton->setVisible(menu != nullptr);
    updateCardStyle();
}

QColor HomePage::accentFor(const QPalette& pal)
{
    const QColor bg = pal.color(QPalette::Window);
    const QColor candidates[] = { pal.color(QPalette::Highlight), pal.color(QPalette::Link), pal.color(QPalette::WindowText) };
    for (const QColor& candidate : candidates) {
        if (contrast(candidate, bg) >= 2.2)
            return candidate;
    }
    return pal.color(QPalette::WindowText);
}

QColor HomePage::textOn(const QColor& background)
{
    const QColor black(0x10, 0x10, 0x10);
    const QColor white(0xFF, 0xFF, 0xFF);
    return contrast(black, background) >= contrast(white, background) ? black : white;
}

void HomePage::updateCardStyle()
{
    if (m_updatingStyle)
        return;
    m_updatingStyle = true;

    const QPalette pal = palette();
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = accentFor(pal);
    const QColor accentText = textOn(accent);
    const QColor muted = mix(fg, bg, 0.4);
    const QColor border = mix(bg, fg, 0.15);
    const bool splitPlay = m_playMenuButton->menu() != nullptr;

    const QString playRadius = splitPlay ? QStringLiteral("10px 0 0 10px") : QStringLiteral("10px");

    const QString qss =
        QStringLiteral(
            "#homeTitle { color: %1; font-size: 34px; font-weight: 800; background: transparent; }"
            "#homeSubtitle { color: %2; font-size: 15px; background: transparent; }"
            "#launchCard { background-color: %3; border: 1px solid %4; border-radius: 14px; }"
            "#launchCardIcon { background-color: %5; border-radius: 10px; }"
            "#launchCardName { color: %1; font-size: 17px; font-weight: 700; background: transparent; }"
            "#launchCardDetails { color: %2; background: transparent; }"
            "#launchCardPicker { color: %2; border: 1px solid %4; border-radius: 6px; padding: 1px 8px; background: transparent; }"
            "#launchCardPicker:hover { color: %1; border-color: %6; }"
            "#launchCardPicker::menu-indicator { image: none; width: 0; }"
            "#launchCardManage { color: %6; border: none; padding: 2px 0; text-align: left; background: transparent; }"
            "#launchCardManage:hover { text-decoration: underline; }"
            "#playButton { background-color: %6; color: %7; border: none; border-radius: %8;"
            " font-size: 18px; font-weight: 800; letter-spacing: 1px; padding: 10px 40px; }"
            "#playButton:hover { background-color: %9; }"
            "#playButton:pressed { background-color: %10; }"
            "#playButton:disabled { background-color: %5; color: %2; }"
            "#playMenuButton { background-color: %6; color: %7; border: none; border-left: 1px solid %10;"
            " border-radius: 0 10px 10px 0; padding: 0 10px; }"
            "#playMenuButton:hover { background-color: %9; }"
            "#playMenuButton:disabled { background-color: %5; }"
            "#playMenuButton::menu-indicator { image: none; width: 0; }")
            .arg(fg.name(), muted.name(), rgba(bg, 215), rgba(border, 200), rgba(border, 160), accent.name(), accentText.name(),
                 playRadius, accent.lighter(112).name())
            .arg(accent.darker(115).name());

    // Only re-apply when something actually changed, so palette/style change
    // notifications caused by our own stylesheet can never loop.
    if (qss != m_appliedStyleSheet) {
        m_appliedStyleSheet = qss;
        setStyleSheet(qss);
    }
    m_updatingStyle = false;
}

void HomePage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        updateCardStyle();
        update();
    } else if (event->type() == QEvent::LanguageChange) {
        retranslate();
    }
    QWidget::changeEvent(event);
}

void HomePage::mouseMoveEvent(QMouseEvent* event)
{
    if (width() > 0 && height() > 0) {
        const QPointF pos = event->position();
        m_parallaxTarget = QPointF(pos.x() / width() * 2.0 - 1.0, pos.y() / height() * 2.0 - 1.0);
        if (!m_animTimer->isActive())
            m_animTimer->start();
    }
    QWidget::mouseMoveEvent(event);
}

void HomePage::leaveEvent(QEvent* event)
{
    m_parallaxTarget = QPointF(0, 0);
    if (!m_animTimer->isActive())
        m_animTimer->start();
    QWidget::leaveEvent(event);
}

void HomePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    update();
}

void HomePage::stepParallax()
{
    const QPointF delta = m_parallaxTarget - m_parallax;
    if (qAbs(delta.x()) < 0.002 && qAbs(delta.y()) < 0.002) {
        m_parallax = m_parallaxTarget;
        m_animTimer->stop();
    } else {
        m_parallax += delta * 0.12;
    }
    update();
}

void HomePage::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);
    paintScenery(painter);
}

void HomePage::paintScenery(QPainter& painter)
{
    const int w = width();
    const int h = height();
    if (w <= 0 || h <= 0)
        return;

    const QPalette pal = palette();
    const QColor bg = pal.color(QPalette::Window);
    const QColor accent = accentFor(pal);
    const QColor fg = pal.color(QPalette::WindowText);
    const bool darkTheme = bg.lightnessF() < 0.5f;

    // Size of one "pixel" of the pixel-art scene.
    const int px = qMax(4, h / 110);

    // Sky: a stepped gradient from deep night at the top to a warm glow at the horizon.
    const QColor skyTop = darkTheme ? bg.darker(160) : mix(bg, fg, 0.08);
    const QColor skyHorizon = mix(bg, accent, darkTheme ? 0.45 : 0.30);
    const int horizon = qMax(1, int(h * 0.62));
    for (int y = 0; y < h; y += px) {
        const qreal t = qBound<qreal>(0.0, qreal(y) / horizon, 1.0);
        // Quantise into bands for the pixel-art look.
        const qreal stepped = qFloor(t * 14.0) / 14.0;
        painter.fillRect(0, y, w, px, mix(skyTop, skyHorizon, stepped * stepped));
    }

    // Stars (only on dark themes, where they read well).
    if (darkTheme) {
        quint32 rng = m_seed;
        const int starCount = (w * h) / 9000;
        const qreal shiftX = -m_parallax.x() * 6.0;
        const qreal shiftY = -m_parallax.y() * 3.0;
        for (int i = 0; i < starCount; ++i) {
            const int sx = int(randomUnit(rng) * (w + 40) - 20 + shiftX);
            const int sy = int(randomUnit(rng) * horizon * 0.75 + shiftY);
            const qreal bright = 0.35 + randomUnit(rng) * 0.65;
            const QColor star = mix(skyTop, QColor(255, 255, 255), bright);
            const int size = (randomUnit(rng) > 0.9) ? px : qMax(2, px / 2);
            painter.fillRect((sx / px) * px, (sy / px) * px, size, size, star);
        }
    }

    // Sun: a chunky pixel disc sitting on the horizon.
    {
        const qreal cx = w * 0.70 - m_parallax.x() * 12.0;
        const qreal cy = horizon - h * 0.06 - m_parallax.y() * 6.0;
        const qreal radius = qMax<qreal>(px * 6.0, h * 0.11);
        const QColor sunCore = mix(accent, QColor(255, 255, 255), 0.35);
        const QColor sunGlow = mix(skyHorizon, accent, 0.55);
        for (qreal sy = cy - radius * 1.6; sy < cy + radius * 1.6; sy += px) {
            for (qreal sx = cx - radius * 1.6; sx < cx + radius * 1.6; sx += px) {
                const qreal d = qSqrt((sx - cx) * (sx - cx) + (sy - cy) * (sy - cy));
                if (d < radius) {
                    painter.fillRect(int(sx / px) * px, int(sy / px) * px, px, px, sunCore);
                } else if (d < radius * 1.25) {
                    painter.fillRect(int(sx / px) * px, int(sy / px) * px, px, px, sunGlow);
                }
            }
        }
    }

    // Three layers of blocky hills: nearer layers are darker and move more.
    struct Layer {
        qreal baseline;   // fraction of the height
        qreal amplitude;  // fraction of the height
        qreal frequency;
        qreal parallax;  // max shift in px
        qreal shade;     // 0 = horizon colour, 1 = darkest
    };
    const Layer layers[] = {
        { 0.60, 0.16, 0.0060, 10.0, 0.45 },
        { 0.70, 0.12, 0.0095, 22.0, 0.70 },
        { 0.82, 0.08, 0.0150, 40.0, 0.92 },
    };
    const QColor deepest = darkTheme ? bg.darker(190) : mix(bg, fg, 0.55);

    quint32 phaseRng = m_seed ^ 0xA5A5A5A5u;
    for (const Layer& layer : layers) {
        const qreal phase1 = randomUnit(phaseRng) * 6.2831;
        const qreal phase2 = randomUnit(phaseRng) * 6.2831;
        const qreal phase3 = randomUnit(phaseRng) * 6.2831;
        const QColor fill = mix(skyHorizon, deepest, layer.shade);
        const QColor rim = fill.lighter(darkTheme ? 125 : 108);
        const qreal shift = -m_parallax.x() * layer.parallax;
        const qreal lift = -m_parallax.y() * layer.parallax * 0.25;

        for (int x = 0; x < w; x += px) {
            const qreal wx = x - shift;
            const qreal n = qSin(wx * layer.frequency + phase1) * 0.6 + qSin(wx * layer.frequency * 2.3 + phase2) * 0.3 +
                            qSin(wx * layer.frequency * 5.1 + phase3) * 0.1;
            int top = int(h * layer.baseline - n * h * layer.amplitude + lift);
            top = (top / px) * px;
            painter.fillRect(x, top, px, h - top, fill);
            painter.fillRect(x, top, px, px, rim);
        }
    }

    // Soft fade at the bottom so the launch card always reads well.
    QLinearGradient fade(0.0, h * 0.55, 0.0, qreal(h));
    QColor clear = bg;
    clear.setAlpha(0);
    QColor solid = bg;
    solid.setAlpha(darkTheme ? 200 : 170);
    fade.setColorAt(0.0, clear);
    fade.setColorAt(1.0, solid);
    painter.fillRect(rect(), QBrush(fade));
}
