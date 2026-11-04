#include "HomePage.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QRadialGradient>
#include <QScrollArea>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

#include <cmath>

#include "BuildConfig.h"
#include "SkinPreview.h"

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

/// Head (with hat layer) of a skin texture, scaled up without smoothing.
QPixmap skinFace(const QImage& skin, int size)
{
    if (skin.isNull() || skin.width() < 48 || skin.height() < 16)
        return QPixmap();
    QImage face(8, 8, QImage::Format_ARGB32);
    face.fill(Qt::transparent);
    {
        QPainter p(&face);
        p.drawImage(0, 0, skin.copy(8, 8, 8, 8));
        p.drawImage(0, 0, skin.copy(40, 8, 8, 8));
    }
    return QPixmap::fromImage(face.scaled(size, size, Qt::IgnoreAspectRatio, Qt::FastTransformation));
}

/// Gold, flame and magenta: the XylarJava Client colours, a bit deeper on light themes.
struct FeatureColors {
    QColor gold, flame, magenta;
};

FeatureColors featureColors(bool dark)
{
    if (dark)
        return { QColor(0xFF, 0xD1, 0x4A), QColor(0xFF, 0x7A, 0x3C), QColor(0xFF, 0x4F, 0xB0) };
    return { QColor(0xC2, 0x6A, 0x00), QColor(0xD8, 0x3A, 0x1A), QColor(0xC0, 0x1C, 0x7A) };
}

/// A label that draws its text with the client gradient while its "featured" property is set.
/// (Stylesheet gradients can't be used for text: they don't stretch across the label.)
class GradientLabel : public QLabel {
   public:
    using QLabel::QLabel;

   protected:
    void paintEvent(QPaintEvent* event) override
    {
        if (!property("featured").toBool() || text().isEmpty()) {
            QLabel::paintEvent(event);
            return;
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setFont(font());
        const QRect area = contentsRect();
        const int textWidth = qMin(area.width(), fontMetrics().horizontalAdvance(text()));
        const FeatureColors colors = featureColors(QApplication::palette(this).color(QPalette::Window).lightnessF() < 0.5f);
        QLinearGradient gradient(area.left(), 0, area.left() + qMax(1, textWidth), 0);
        gradient.setColorAt(0.0, colors.gold);
        gradient.setColorAt(0.5, colors.flame);
        gradient.setColorAt(1.0, colors.magenta);
        painter.setPen(QPen(QBrush(gradient), 1));
        painter.drawText(area, int(alignment()), fontMetrics().elidedText(text(), Qt::ElideRight, area.width()));
    }
};

/// The pin next to the XylarJava Client, tinted with the client's gold.
class PinIcon : public QWidget {
   public:
    explicit PinIcon(QWidget* parent) : QWidget(parent)
    {
        setFixedSize(16, 16);
        setAttribute(Qt::WA_TranslucentBackground, true);
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        const qreal dpr = devicePixelRatioF();
        QPixmap pin = QIcon(QStringLiteral(":/xylar/pin.png")).pixmap(size(), dpr);
        if (pin.isNull())
            return;
        {
            QPainter tint(&pin);
            tint.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tint.fillRect(QRect(QPoint(0, 0), pin.size()),
                          featureColors(QApplication::palette(this).color(QPalette::Window).lightnessF() < 0.5f).gold);
        }
        QPainter painter(this);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(rect(), pin);
    }
};

}  // namespace

// ---------------------------------------------------------------------------
// HeroScene: the big rounded card with the animated pixel-art landscape.
// ---------------------------------------------------------------------------
class HeroScene : public QWidget {
   public:
    explicit HeroScene(QWidget* parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("heroScene"));
        setMouseTracking(true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setMinimumHeight(330);
        m_timer = new QTimer(this);
        m_timer->setInterval(33);
        QObject::connect(m_timer, &QTimer::timeout, this, [this] { tick(); });
        m_clock.start();
    }

    void setAnimating(bool on)
    {
        if (on) {
            m_timer->start();
        } else {
            m_timer->stop();
            // Nothing is drawn while the scene is still, so hand its buffer back until it is.
            m_backdrop = QPixmap();
            m_dirty = true;
        }
    }

    void invalidate()
    {
        m_dirty = true;
        update();
    }

   protected:
    void paintEvent(QPaintEvent*) override
    {
        const qreal dpr = devicePixelRatioF();
        const QSize pixelSize = size() * dpr;
        if (m_dirty || m_backdrop.size() != pixelSize || m_backdropParallax != m_parallax) {
            // Repaint into the same buffer: a new one per frame churns megabytes while the mouse moves.
            if (m_backdrop.size() != pixelSize) {
                m_backdrop = QPixmap();
                m_backdrop = QPixmap(pixelSize);
            }
            m_backdrop.setDevicePixelRatio(dpr);
            m_backdrop.fill(Qt::transparent);
            QPainter bp(&m_backdrop);
            bp.setRenderHint(QPainter::Antialiasing, false);
            paintBackdrop(bp);
            m_backdropParallax = m_parallax;
            m_dirty = false;
        }

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QPainterPath clip;
        clip.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 18, 18);
        painter.setClipPath(clip);
        painter.drawPixmap(0, 0, m_backdrop);
        painter.setRenderHint(QPainter::Antialiasing, false);
        paintLiveLayer(painter);

        // Soft fade at the bottom so the instance bar reads well.
        const QColor bg = QApplication::palette(this).color(QPalette::Window);
        QLinearGradient fade(0.0, height() * 0.55, 0.0, qreal(height()));
        QColor clear = bg;
        clear.setAlpha(0);
        QColor solid = bg;
        solid.setAlpha(225);
        fade.setColorAt(0.0, clear);
        fade.setColorAt(1.0, solid);
        painter.fillRect(rect(), QBrush(fade));

        // Hairline border on top of everything.
        painter.setClipping(false);
        painter.setRenderHint(QPainter::Antialiasing, true);
        QColor border = QApplication::palette(this).color(QPalette::WindowText);
        border.setAlpha(28);
        painter.setPen(QPen(border, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(clip);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (width() > 0 && height() > 0) {
            const QPointF pos = event->position();
            m_target = QPointF(pos.x() / width() * 2.0 - 1.0, pos.y() / height() * 2.0 - 1.0);
        }
        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_target = QPointF(0, 0);
        QWidget::leaveEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override
    {
        QWidget::resizeEvent(event);
        m_dirty = true;
    }

   private:
    void tick()
    {
        const QPointF delta = m_target - m_parallax;
        if (qAbs(delta.x()) < 0.002 && qAbs(delta.y()) < 0.002)
            m_parallax = m_target;
        else
            m_parallax += delta * 0.12;
        update();
    }

    void paintBackdrop(QPainter& painter)
    {
        const int w = width();
        const int h = height();
        if (w <= 0 || h <= 0)
            return;

        const QPalette pal = QApplication::palette(this);
        const QColor bg = pal.color(QPalette::Window);
        const QColor accent = HomePage::accentFor(pal);
        const QColor fg = pal.color(QPalette::WindowText);
        const bool darkTheme = bg.lightnessF() < 0.5f;
        const int px = qMax(4, h / 90);

        const QColor skyTop = darkTheme ? bg.darker(160) : mix(bg, fg, 0.08);
        const QColor skyHorizon = mix(bg, accent, darkTheme ? 0.48 : 0.32);
        const int horizon = qMax(1, int(h * 0.66));
        for (int y = 0; y < h; y += px) {
            const qreal t = qBound<qreal>(0.0, qreal(y) / horizon, 1.0);
            const qreal stepped = qFloor(t * 14.0) / 14.0;
            painter.fillRect(0, y, w, px, mix(skyTop, skyHorizon, stepped * stepped));
        }

        // Pixel sun, off to the side so the character stands in front of the sky.
        {
            const qreal cx = w * 0.72 - m_parallax.x() * 12.0;
            const qreal cy = horizon - h * 0.04 - m_parallax.y() * 6.0;
            const qreal radius = qMax<qreal>(px * 6.0, h * 0.13);
            const QColor sunCore = mix(accent, QColor(255, 255, 255), 0.35);
            const QColor sunGlow = mix(skyHorizon, accent, 0.55);
            for (qreal sy = cy - radius * 1.6; sy < cy + radius * 1.6; sy += px) {
                for (qreal sx = cx - radius * 1.6; sx < cx + radius * 1.6; sx += px) {
                    const qreal d = qSqrt((sx - cx) * (sx - cx) + (sy - cy) * (sy - cy));
                    if (d < radius)
                        painter.fillRect(int(sx / px) * px, int(sy / px) * px, px, px, sunCore);
                    else if (d < radius * 1.25)
                        painter.fillRect(int(sx / px) * px, int(sy / px) * px, px, px, sunGlow);
                }
            }
        }

        struct Layer {
            qreal baseline;
            qreal amplitude;
            qreal frequency;
            qreal parallax;
            qreal shade;
        };
        const Layer layers[] = {
            { 0.62, 0.15, 0.0060, 10.0, 0.45 },
            { 0.72, 0.11, 0.0095, 22.0, 0.70 },
            { 0.86, 0.06, 0.0150, 40.0, 0.92 },
        };
        const QColor deepest = darkTheme ? bg.darker(190) : mix(bg, fg, 0.55);
        quint32 phaseRng = 0x5EED1234u ^ 0xA5A5A5A5u;
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
    }

    void paintLiveLayer(QPainter& painter)
    {
        const int w = width();
        const int h = height();
        if (w <= 0 || h <= 0)
            return;
        const QPalette pal = QApplication::palette(this);
        const QColor bg = pal.color(QPalette::Window);
        const QColor fg = pal.color(QPalette::WindowText);
        const bool darkTheme = bg.lightnessF() < 0.5f;
        const int px = qMax(4, h / 90);
        const int horizon = qMax(1, int(h * 0.66));
        const qreal seconds = m_clock.elapsed() / 1000.0;
        const QColor skyTop = darkTheme ? bg.darker(160) : mix(bg, fg, 0.08);

        if (darkTheme) {
            quint32 rng = 0x5EED1234u;
            const int starCount = (w * h) / 7000;
            for (int i = 0; i < starCount; ++i) {
                const int sx = int(randomUnit(rng) * (w + 40) - 20 - m_parallax.x() * 6.0);
                const int sy = int(randomUnit(rng) * horizon * 0.5 - m_parallax.y() * 3.0);
                const qreal base = 0.35 + randomUnit(rng) * 0.65;
                const qreal speed = 0.6 + randomUnit(rng) * 1.8;
                const qreal offset = randomUnit(rng) * 6.2831;
                const int size = (randomUnit(rng) > 0.9) ? px : qMax(2, px / 2);
                const qreal twinkle = 0.65 + 0.35 * qSin(seconds * speed + offset);
                painter.fillRect((sx / px) * px, (sy / px) * px, size, size, mix(skyTop, QColor(255, 255, 255), base * twinkle));
            }
        }

        quint32 rng = 0x5EED1234u ^ 0x0C10D5u;
        const QColor cloudColor = darkTheme ? mix(skyTop, fg, 0.16) : mix(bg, QColor(255, 255, 255), 0.7);
        for (int i = 0; i < 4; ++i) {
            const qreal cloudW = (10 + randomUnit(rng) * 12) * px;
            const qreal speed = (4 + randomUnit(rng) * 6) * px / 10.0;
            const qreal y = h * (0.08 + randomUnit(rng) * 0.22) - m_parallax.y() * 4.0;
            const qreal start = randomUnit(rng) * (w + cloudW);
            const qreal travel = std::fmod(start + seconds * speed, w + cloudW * 2.0);
            const qreal x = travel - cloudW - m_parallax.x() * 8.0;
            const int left = int(x / px) * px;
            const int top = int(y / px) * px;
            const int cw = int(cloudW / px) * px;
            QColor fill = cloudColor;
            fill.setAlpha(darkTheme ? 120 : 170);
            painter.fillRect(left, top + px, cw, px * 2, fill);
            painter.fillRect(left + px * 2, top, cw - px * 5, px, fill);
            painter.fillRect(left + px * 4, top - px, cw / 3, px, fill);
        }
    }

    QTimer* m_timer = nullptr;
    QElapsedTimer m_clock;
    QPointF m_parallax;
    QPointF m_target;
    QPixmap m_backdrop;
    QPointF m_backdropParallax;
    bool m_dirty = true;
};

// ---------------------------------------------------------------------------
// HomePage
// ---------------------------------------------------------------------------

QFrame* HomePage::makeCard(const QString& objectName)
{
    auto card = new QFrame();
    card->setObjectName(objectName);
    card->setProperty("card", true);
    return card;
}

HomePage::HomePage(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("homePage"));
    setAttribute(Qt::WA_StyledBackground, false);

    auto scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("homeScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto content = new QWidget(scroll);
    content->setObjectName(QStringLiteral("homeContent"));
    scroll->setWidget(content);

    auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    // --- Greeting ----------------------------------------------------------
    m_greetingFace = new QLabel(content);
    m_greetingFace->setFixedSize(34, 34);
    m_greeting = new QLabel(content);
    m_greeting->setObjectName(QStringLiteral("homeGreeting"));
    m_greeting->setTextFormat(Qt::PlainText);
    m_greetingSub = new QLabel(content);
    m_greetingSub->setObjectName(QStringLiteral("homeGreetingSub"));

    auto greetingText = new QVBoxLayout();
    greetingText->setSpacing(0);
    greetingText->addWidget(m_greeting);
    greetingText->addWidget(m_greetingSub);
    auto greetingRow = new QHBoxLayout();
    greetingRow->setSpacing(12);
    greetingRow->addWidget(m_greetingFace, 0, Qt::AlignVCenter);
    greetingRow->addLayout(greetingText, 1);

    // --- Hero ----------------------------------------------------------------
    m_hero = new HeroScene(content);

    m_nameplate = new QLabel(m_hero);
    m_nameplate->setObjectName(QStringLiteral("heroNameplate"));
    m_nameplate->setAlignment(Qt::AlignCenter);
    m_nameplate->setTextFormat(Qt::PlainText);
    m_skin = new SkinPreview(m_hero);
    m_skin->setMinimumSize(120, 200);
    m_skin->setMaximumSize(170, 260);

    auto heroBar = new QFrame(m_hero);
    heroBar->setObjectName(QStringLiteral("heroBar"));
    m_iconLabel = new QLabel(heroBar);
    m_iconLabel->setObjectName(QStringLiteral("heroIcon"));
    m_iconLabel->setFixedSize(46, 46);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel = new GradientLabel(heroBar);
    m_nameLabel->setObjectName(QStringLiteral("heroName"));
    m_nameLabel->setTextFormat(Qt::PlainText);
    m_detailsLabel = new QLabel(heroBar);
    m_detailsLabel->setObjectName(QStringLiteral("heroDetails"));
    m_detailsLabel->setTextFormat(Qt::PlainText);
    m_pickerButton = new QToolButton(heroBar);
    m_pickerButton->setObjectName(QStringLiteral("heroPicker"));
    m_pickerButton->setPopupMode(QToolButton::InstantPopup);
    m_pickerButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    m_pickerButton->setCursor(Qt::PointingHandCursor);

    m_playButton = new QPushButton(heroBar);
    m_playButton->setObjectName(QStringLiteral("playButton"));
    m_playButton->setCursor(Qt::PointingHandCursor);
    m_playButton->setMinimumHeight(50);
    m_playButton->setMinimumWidth(180);
    connect(m_playButton, &QPushButton::clicked, this, [this] {
        if (!m_hasInstance)
            emit createInstanceRequested();
        else if (m_running)
            emit stopRequested();
        else
            emit playRequested();
    });
    m_playMenuButton = new QToolButton(heroBar);
    m_playMenuButton->setObjectName(QStringLiteral("playMenuButton"));
    m_playMenuButton->setPopupMode(QToolButton::InstantPopup);
    m_playMenuButton->setArrowType(Qt::DownArrow);
    m_playMenuButton->setMinimumHeight(50);
    m_playMenuButton->setCursor(Qt::PointingHandCursor);
    m_playMenuButton->setVisible(false);

    auto nameRow = new QHBoxLayout();
    nameRow->setSpacing(8);
    nameRow->addWidget(m_nameLabel);
    nameRow->addWidget(m_pickerButton);
    nameRow->addStretch(1);
    auto heroText = new QVBoxLayout();
    heroText->setSpacing(2);
    heroText->addLayout(nameRow);
    heroText->addWidget(m_detailsLabel);
    auto playRow = new QHBoxLayout();
    playRow->setSpacing(0);
    playRow->addWidget(m_playButton);
    playRow->addWidget(m_playMenuButton);
    auto heroBarLayout = new QHBoxLayout(heroBar);
    heroBarLayout->setContentsMargins(18, 14, 18, 14);
    heroBarLayout->setSpacing(14);
    heroBarLayout->addWidget(m_iconLabel);
    heroBarLayout->addLayout(heroText, 1);
    heroBarLayout->addLayout(playRow);

    auto skinColumn = new QVBoxLayout();
    skinColumn->setSpacing(4);
    skinColumn->addWidget(m_nameplate, 0, Qt::AlignHCenter);
    skinColumn->addWidget(m_skin, 0, Qt::AlignHCenter);
    auto heroLayout = new QVBoxLayout(m_hero);
    heroLayout->setContentsMargins(16, 18, 16, 16);
    heroLayout->setSpacing(0);
    heroLayout->addStretch(1);
    heroLayout->addLayout(skinColumn);
    heroLayout->addSpacing(6);
    heroLayout->addWidget(heroBar);

    // --- Account card --------------------------------------------------------
    auto accountCard = makeCard(QStringLiteral("accountCard"));
    m_accountFace = new QLabel(accountCard);
    m_accountFace->setObjectName(QStringLiteral("accountFace"));
    m_accountFace->setFixedSize(64, 64);
    m_accountFace->setAlignment(Qt::AlignCenter);
    m_accountName = new QLabel(accountCard);
    m_accountName->setObjectName(QStringLiteral("accountName"));
    m_accountName->setTextFormat(Qt::PlainText);
    m_accountNote = new QLabel(accountCard);
    m_accountNote->setObjectName(QStringLiteral("cardMuted"));
    m_accountNote->setWordWrap(true);
    m_accountNote->setTextFormat(Qt::PlainText);
    m_skinButton = new QPushButton(accountCard);
    m_skinButton->setObjectName(QStringLiteral("cardButton"));
    m_skinButton->setCursor(Qt::PointingHandCursor);
    connect(m_skinButton, &QPushButton::clicked, this, [this] {
        if (m_hasPlayer)
            emit skinRequested();
        else
            emit manageAccountsRequested();
    });
    m_accountsButton = new QPushButton(accountCard);
    m_accountsButton->setObjectName(QStringLiteral("cardButton"));
    m_accountsButton->setCursor(Qt::PointingHandCursor);
    connect(m_accountsButton, &QPushButton::clicked, this, &HomePage::manageAccountsRequested);

    auto accountTop = new QHBoxLayout();
    accountTop->setSpacing(14);
    accountTop->addWidget(m_accountFace);
    auto accountText = new QVBoxLayout();
    accountText->setSpacing(2);
    accountText->addStretch(1);
    accountText->addWidget(m_accountName);
    accountText->addWidget(m_accountNote);
    accountText->addStretch(1);
    accountTop->addLayout(accountText, 1);
    auto accountButtons = new QHBoxLayout();
    accountButtons->setSpacing(8);
    accountButtons->addWidget(m_skinButton);
    accountButtons->addWidget(m_accountsButton);
    auto accountLayout = new QVBoxLayout(accountCard);
    accountLayout->setContentsMargins(18, 18, 18, 18);
    accountLayout->setSpacing(14);
    accountLayout->addLayout(accountTop);
    accountLayout->addLayout(accountButtons);

    // --- Updates card --------------------------------------------------------
    auto updatesCard = makeCard(QStringLiteral("updatesCard"));
    m_logoLabel = new QLabel(updatesCard);
    m_logoLabel->setFixedSize(40, 40);
    m_versionLabel = new QLabel(updatesCard);
    m_versionLabel->setObjectName(QStringLiteral("cardTitle"));
    m_versionLabel->setTextFormat(Qt::PlainText);
    m_versionStatus = new QLabel(updatesCard);
    m_versionStatus->setObjectName(QStringLiteral("cardMuted"));
    m_versionStatus->setWordWrap(true);
    m_versionStatus->setTextFormat(Qt::PlainText);
    m_updatesButton = new QPushButton(updatesCard);
    m_updatesButton->setObjectName(QStringLiteral("cardButton"));
    m_updatesButton->setCursor(Qt::PointingHandCursor);
    connect(m_updatesButton, &QPushButton::clicked, this, &HomePage::checkUpdatesRequested);
    auto updatesTop = new QHBoxLayout();
    updatesTop->setSpacing(12);
    updatesTop->addWidget(m_logoLabel);
    auto updatesText = new QVBoxLayout();
    updatesText->setSpacing(2);
    updatesText->addWidget(m_versionLabel);
    updatesText->addWidget(m_versionStatus);
    updatesTop->addLayout(updatesText, 1);
    auto updatesLayout = new QVBoxLayout(updatesCard);
    updatesLayout->setContentsMargins(18, 16, 18, 16);
    updatesLayout->setSpacing(12);
    updatesLayout->addLayout(updatesTop);
    updatesLayout->addWidget(m_updatesButton);

    // --- Stats card ----------------------------------------------------------
    auto statsCard = makeCard(QStringLiteral("statsCard"));
    m_statsTitle = new QLabel(statsCard);
    m_statsTitle->setObjectName(QStringLiteral("cardTitle"));
    auto makeStat = [statsCard](QLabel*& value, QLabel*& caption) {
        auto tile = new QFrame(statsCard);
        tile->setObjectName(QStringLiteral("statTile"));
        value = new QLabel(tile);
        value->setObjectName(QStringLiteral("statValue"));
        caption = new QLabel(tile);
        caption->setObjectName(QStringLiteral("cardMuted"));
        auto tileLayout = new QVBoxLayout(tile);
        tileLayout->setContentsMargins(14, 12, 14, 12);
        tileLayout->setSpacing(0);
        tileLayout->addWidget(value);
        tileLayout->addWidget(caption);
        return tile;
    };
    auto statRow = new QHBoxLayout();
    statRow->setSpacing(10);
    statRow->addWidget(makeStat(m_statInstances, m_statInstancesCaption), 1);
    statRow->addWidget(makeStat(m_statPlaytime, m_statPlaytimeCaption), 1);
    m_statFavourite = new QLabel(statsCard);
    m_statFavourite->setObjectName(QStringLiteral("cardMuted"));
    m_statFavourite->setTextFormat(Qt::PlainText);
    m_statFavourite->setWordWrap(true);
    auto statsLayout = new QVBoxLayout(statsCard);
    statsLayout->setContentsMargins(18, 16, 18, 16);
    statsLayout->setSpacing(12);
    statsLayout->addWidget(m_statsTitle);
    statsLayout->addLayout(statRow);
    statsLayout->addWidget(m_statFavourite);

    auto rightColumn = new QVBoxLayout();
    rightColumn->setSpacing(16);
    rightColumn->addWidget(accountCard);
    rightColumn->addWidget(statsCard);
    rightColumn->addWidget(updatesCard);
    rightColumn->addStretch(1);

    // --- Recently played -------------------------------------------------------
    m_recentTitle = new QLabel(content);
    m_recentTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_recentLayout = new QVBoxLayout();
    m_recentLayout->setSpacing(10);
    m_recentEmpty = new QLabel(content);
    m_recentEmpty->setObjectName(QStringLiteral("cardMuted"));
    m_manageButton = new QPushButton(content);
    m_manageButton->setObjectName(QStringLiteral("accentButton"));
    m_manageButton->setCursor(Qt::PointingHandCursor);
    m_manageButton->setMinimumHeight(44);
    connect(m_manageButton, &QPushButton::clicked, this, &HomePage::manageInstancesRequested);
    m_addButton = new QPushButton(content);
    m_addButton->setObjectName(QStringLiteral("cardButton"));
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setMinimumHeight(44);
    connect(m_addButton, &QPushButton::clicked, this, &HomePage::createInstanceRequested);
    auto bottomButtons = new QHBoxLayout();
    bottomButtons->setSpacing(12);
    bottomButtons->addWidget(m_manageButton, 1);
    bottomButtons->addWidget(m_addButton, 1);

    auto leftColumn = new QVBoxLayout();
    leftColumn->setSpacing(14);
    leftColumn->addWidget(m_hero);
    leftColumn->addSpacing(6);
    leftColumn->addWidget(m_recentTitle);
    leftColumn->addLayout(m_recentLayout);
    leftColumn->addWidget(m_recentEmpty);
    leftColumn->addLayout(bottomButtons);
    leftColumn->addStretch(1);

    auto grid = new QHBoxLayout();
    grid->setSpacing(20);
    grid->addLayout(leftColumn, 1);
    auto rightHolder = new QWidget(content);
    rightHolder->setFixedWidth(320);
    rightHolder->setLayout(rightColumn);
    rightColumn->setContentsMargins(0, 0, 0, 0);
    grid->addWidget(rightHolder, 0, Qt::AlignTop);
    m_grid = grid;
    m_rightHolder = rightHolder;

    auto page = new QVBoxLayout(content);
    page->setContentsMargins(32, 26, 32, 26);
    page->setSpacing(22);
    page->addLayout(greetingRow);
    page->addLayout(grid, 1);

    retranslate();
    updateStyle();
    showNoInstance(false);
    showNoPlayer();
    setVersionInfo(BuildConfig.printableVersionString(), QString());
    rebuildRecent();
}

void HomePage::retranslate()
{
    m_greetingSub->setText(tr("Ready when you are."));
    m_pickerButton->setText(tr("Switch"));
    m_pickerButton->setToolTip(tr("Choose what to play"));
    m_playMenuButton->setToolTip(tr("More ways to launch"));
    m_skinButton->setText(m_hasPlayer ? tr("Skin && cape") : tr("Add an account"));
    m_accountsButton->setText(tr("Accounts"));
    m_updatesButton->setText(tr("Check for updates"));
    m_recentTitle->setText(tr("Recently played"));
    m_recentEmpty->setText(tr("Nothing here yet. Play an instance and it shows up here."));
    m_manageButton->setText(tr("Manage all instances"));
    m_statsTitle->setText(tr("Your stats"));
    setStats(m_instanceCount, m_secondsPlayed, m_mostPlayed);
    m_addButton->setText(QStringLiteral("+  ") + tr("Add instance"));
}

void HomePage::setLogo(const QIcon& logo)
{
    m_logoLabel->setPixmap(logo.pixmap(40, 40));
}

void HomePage::showInstance(const QIcon& icon, const QString& name, const QString& details, bool canLaunch, bool running, bool featured)
{
    m_hasInstance = true;
    m_nameLabel->setProperty("featured", featured);
    m_nameLabel->update();
    m_instancesExist = true;
    m_running = running;
    m_iconLabel->setPixmap(icon.pixmap(36, 36));
    m_nameLabel->setText(name);
    m_detailsLabel->setText(details);
    m_pickerButton->setVisible(true);
    m_playButton->setProperty("running", running);
    if (running) {
        m_playButton->setText(tr("STOP"));
        m_playButton->setEnabled(true);
    } else {
        m_playButton->setText(tr("PLAY"));
        m_playButton->setEnabled(canLaunch);
    }
    m_playMenuButton->setEnabled(!running && canLaunch);
    // The launch options make no sense while the game is running, so Stop gets the whole button.
    m_playMenuButton->setVisible(m_playMenuButton->menu() != nullptr && !running);
    updateStyle();
}

void HomePage::showNoInstance(bool instancesExist)
{
    m_hasInstance = false;
    m_nameLabel->setProperty("featured", false);
    m_nameLabel->update();
    m_instancesExist = instancesExist;
    m_running = false;
    m_iconLabel->setPixmap(QIcon::fromTheme(QStringLiteral("new")).pixmap(36, 36));
    m_pickerButton->setVisible(instancesExist);
    m_playMenuButton->setEnabled(false);
    m_playMenuButton->setVisible(false);
    m_playButton->setProperty("running", false);
    if (instancesExist) {
        m_nameLabel->setText(tr("Nothing picked yet"));
        m_detailsLabel->setText(tr("Hit Switch and choose what you feel like playing."));
        m_playButton->setText(tr("PLAY"));
        m_playButton->setEnabled(false);
    } else {
        m_nameLabel->setText(tr("No instances yet"));
        m_detailsLabel->setText(tr("Make your first one and you're good to go."));
        m_playButton->setText(tr("CREATE"));
        m_playButton->setEnabled(true);
    }
    updateStyle();
}

void HomePage::showPlayer(const QString& name, const QString& note, const QImage& skin, bool slim)
{
    m_hasPlayer = true;
    m_skin->setSkin(skin, slim);
    m_greeting->setText(tr("Welcome back, %1!").arg(name));
    m_nameplate->setText(name);
    m_nameplate->setVisible(true);
    m_accountName->setText(name);
    m_accountNote->setText(note);
    const QPixmap face = skinFace(skin, 64);
    m_accountFace->setPixmap(face.isNull() ? QIcon::fromTheme(QStringLiteral("noaccount")).pixmap(48, 48) : face);
    const QPixmap smallFace = skinFace(skin, 34);
    m_greetingFace->setPixmap(smallFace);
    m_greetingFace->setVisible(!smallFace.isNull());
    m_skinButton->setText(tr("Skin && cape"));
}

void HomePage::showNoPlayer()
{
    m_hasPlayer = false;
    m_skin->setSkin(QImage(), false);
    m_greeting->setText(tr("Welcome to %1!").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    m_nameplate->setVisible(false);
    m_accountName->setText(tr("No account yet"));
    m_accountNote->setText(tr("Sign in and your skin shows up right here."));
    m_accountFace->setPixmap(QIcon::fromTheme(QStringLiteral("noaccount")).pixmap(48, 48));
    m_greetingFace->setVisible(false);
    m_skinButton->setText(tr("Add an account"));
}

void HomePage::setRecentInstances(const QList<RecentInstance>& instances)
{
    m_recent = instances;
    rebuildRecent();
}

void HomePage::rebuildRecent()
{
    while (QLayoutItem* item = m_recentLayout->takeAt(0)) {
        if (QWidget* w = item->widget()) {
            w->hide();
            w->deleteLater();
        }
        delete item;
    }
    m_recentEmpty->setVisible(m_recent.isEmpty());

    for (const RecentInstance& entry : m_recent) {
        const bool needsSetup = entry.featured && entry.id.isEmpty();
        auto row = new QFrame();
        row->setObjectName(entry.featured ? QStringLiteral("featuredRow") : QStringLiteral("recentRow"));
        row->setProperty("card", true);
        row->setProperty("running", entry.running);

        auto icon = new QLabel(row);
        icon->setObjectName(QStringLiteral("recentIcon"));
        icon->setFixedSize(44, 44);
        icon->setAlignment(Qt::AlignCenter);
        icon->setPixmap(entry.icon.pixmap(34, 34));
        auto name = new GradientLabel(entry.name, row);
        name->setProperty("featured", entry.featured);
        name->setObjectName(entry.featured ? QStringLiteral("featuredName") : QStringLiteral("recentName"));
        name->setTextFormat(Qt::PlainText);
        auto nameRow = new QHBoxLayout();
        nameRow->setSpacing(8);
        nameRow->addWidget(name);
        if (entry.featured) {
            auto pin = new PinIcon(row);
            pin->setToolTip(tr("Pinned"));
            nameRow->addWidget(pin, 0, Qt::AlignVCenter);
        }
        nameRow->addStretch(1);
        auto details = new QLabel(entry.details, row);
        details->setObjectName(QStringLiteral("cardMuted"));
        details->setTextFormat(Qt::PlainText);
        auto text = new QVBoxLayout();
        text->setSpacing(2);
        text->addLayout(nameRow);
        text->addWidget(details);

        const QString id = entry.id;
        auto play = new QPushButton(entry.running ? tr("Stop") : needsSetup ? tr("Set up") : tr("Play"), row);
        if (entry.running)
            play->setObjectName(QStringLiteral("stopButton"));
        else
            play->setObjectName(entry.featured ? QStringLiteral("featuredPlayButton") : QStringLiteral("rowPlayButton"));
        play->setCursor(Qt::PointingHandCursor);
        play->setMinimumSize(96, 38);
        play->setEnabled(entry.running || entry.canLaunch || needsSetup);
        if (needsSetup)
            connect(play, &QPushButton::clicked, this, &HomePage::setupClientRequested);
        else if (entry.running)
            connect(play, &QPushButton::clicked, this, [this, id] { emit stopInstanceRequested(id); });
        else
            connect(play, &QPushButton::clicked, this, [this, id] { emit playInstanceRequested(id); });
        auto edit = new QPushButton(QIcon::fromTheme(QStringLiteral("instance-settings")), QString(), row);
        edit->setObjectName(QStringLiteral("iconButton"));
        edit->setCursor(Qt::PointingHandCursor);
        edit->setFixedSize(38, 38);
        edit->setToolTip(tr("Edit instance"));
        connect(edit, &QPushButton::clicked, this, [this, id] { emit editInstanceRequested(id); });
        edit->setVisible(!needsSetup);
        // Right click: the same actions plus Repair, for an instance that won't start or keeps crashing.
        if (!needsSetup) {
            row->setContextMenuPolicy(Qt::CustomContextMenu);
            connect(row, &QWidget::customContextMenuRequested, this, [this, row, id, running = entry.running](const QPoint& pos) {
                QMenu menu(row);
                menu.addAction(tr("Edit instance"), this, [this, id] { emit editInstanceRequested(id); });
                auto repair = menu.addAction(tr("Repair..."), this, [this, id] { emit repairInstanceRequested(id); });
                repair->setEnabled(!running);
                menu.exec(row->mapToGlobal(pos));
            });
        }

        auto layout = new QHBoxLayout(row);
        layout->setContentsMargins(14, 12, 14, 12);
        layout->setSpacing(14);
        layout->addWidget(icon);
        layout->addLayout(text, 1);
        layout->addWidget(play);
        layout->addWidget(edit);
        m_recentLayout->addWidget(row);
    }
    updateStyle();
}

void HomePage::setVersionInfo(const QString& version, const QString& status)
{
    m_versionLabel->setText(QStringLiteral("%1 %2").arg(BuildConfig.LAUNCHER_DISPLAYNAME, version));
    m_versionStatus->setText(status.isEmpty() ? tr("New versions install themselves when you start the launcher.") : status);
}

void HomePage::setStats(int instanceCount, qint64 secondsPlayed, const QString& mostPlayed)
{
    m_instanceCount = instanceCount;
    m_secondsPlayed = secondsPlayed;
    m_mostPlayed = mostPlayed;

    m_statInstances->setText(QString::number(instanceCount));
    m_statInstancesCaption->setText(instanceCount == 1 ? tr("instance") : tr("instances"));

    const qint64 hours = secondsPlayed / 3600;
    const qint64 minutes = (secondsPlayed % 3600) / 60;
    if (hours > 0)
        m_statPlaytime->setText(tr("%1h %2m").arg(hours).arg(minutes));
    else
        m_statPlaytime->setText(tr("%1m").arg(minutes));
    m_statPlaytimeCaption->setText(tr("played"));

    if (mostPlayed.isEmpty())
        m_statFavourite->setText(tr("Play something and your favourite shows up here."));
    else
        m_statFavourite->setText(tr("Most played: %1").arg(mostPlayed));
}

void HomePage::setInstanceMenu(QMenu* menu)
{
    m_pickerButton->setMenu(menu);
}

void HomePage::setLaunchMenu(QMenu* menu)
{
    m_playMenuButton->setMenu(menu);
    m_playMenuButton->setVisible(menu != nullptr && m_hasInstance && !m_running);
    updateStyle();
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

void HomePage::paintEvent(QPaintEvent*)
{
    // Page background: the window colour with two soft glows for depth.
    QPainter painter(this);
    const QPalette pal = QApplication::palette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor accent = accentFor(pal);
    painter.fillRect(rect(), bg);

    QColor warm = accent;
    warm.setAlpha(bg.lightnessF() < 0.5f ? 34 : 22);
    QRadialGradient topRight(QPointF(width() * 0.85, -height() * 0.1), qMax(width(), height()) * 0.7);
    topRight.setColorAt(0.0, warm);
    topRight.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect(), QBrush(topRight));

    QColor cool = mix(accent, QColor(0x5B, 0x6C, 0xFF), 0.75);
    cool.setAlpha(bg.lightnessF() < 0.5f ? 26 : 16);
    QRadialGradient bottomLeft(QPointF(-width() * 0.05, height() * 1.05), qMax(width(), height()) * 0.6);
    bottomLeft.setColorAt(0.0, cool);
    bottomLeft.setColorAt(1.0, Qt::transparent);
    painter.fillRect(rect(), QBrush(bottomLeft));
}

void HomePage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange ||
        event->type() == QEvent::StyleChange || event->type() == QEvent::ParentChange) {
        // Widgets with their own stylesheet are skipped by Qt's palette propagation, so colours come
        // straight from the application palette (the active theme) and are refreshed after a change.
        if (!m_restylePending) {
            m_restylePending = true;
            QTimer::singleShot(0, this, [this] {
                m_restylePending = false;
                updateStyle();
                if (m_hero)
                    m_hero->invalidate();
                if (m_skin)
                    m_skin->update();
                update();
            });
        }
    } else if (event->type() == QEvent::LanguageChange) {
        retranslate();
    } else if (event->type() == QEvent::ActivationChange) {
        // Only animate while the launcher is the active window: no CPU spent in the background.
        if (m_hero)
            m_hero->setAnimating(isActiveWindow() && isVisible());
    }
    QWidget::changeEvent(event);
}

void HomePage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Half a screen wide there's no room for two columns: the account, stats and updates cards go
    // under the instances instead of being cut off on the right.
    const bool stacked = width() < 960;
    if (!m_grid || stacked == (m_grid->direction() == QBoxLayout::TopToBottom))
        return;
    m_grid->setDirection(stacked ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    if (stacked) {
        m_rightHolder->setMinimumWidth(0);
        m_rightHolder->setMaximumWidth(QWIDGETSIZE_MAX);
        m_grid->setAlignment(m_rightHolder, Qt::Alignment());
    } else {
        m_rightHolder->setFixedWidth(320);
        m_grid->setAlignment(m_rightHolder, Qt::AlignTop);
    }
}

void HomePage::showEvent(QShowEvent* event)
{
    if (m_hero)
        m_hero->setAnimating(isActiveWindow());
    QWidget::showEvent(event);
}

void HomePage::hideEvent(QHideEvent* event)
{
    // Another page (or a minimised window): stop the scene and free its buffer.
    if (m_hero)
        m_hero->setAnimating(false);
    QWidget::hideEvent(event);
}

void HomePage::updateStyle()
{
    if (m_updatingStyle)
        return;
    m_updatingStyle = true;

    const QPalette pal = QApplication::palette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = accentFor(pal);
    const QColor accentText = textOn(accent);
    const bool dark = bg.lightnessF() < 0.5f;
    const QColor muted = mix(fg, bg, 0.42);
    const QColor card = dark ? bg.lighter(118) : bg.darker(104);
    const QColor cardHover = dark ? bg.lighter(132) : bg.darker(110);
    const QColor danger(0xE5, 0x48, 0x4D);
    const QColor splitColor = accent.darker(115);

    const QString playRadius = !m_playMenuButton->isHidden()
                                   ? QStringLiteral("border-top-left-radius: 12px; border-bottom-left-radius: 12px;"
                                                    " border-top-right-radius: 0; border-bottom-right-radius: 0;")
                                   : QStringLiteral("border-radius: 12px;");
    const bool running = m_playButton->property("running").toBool();
    const QColor playColor = running ? danger : accent;
    const bool clientSelected = m_hasInstance && m_nameLabel->property("featured").toBool();

    QString qss;
    qss += QStringLiteral("#homeScroll, #homeContent { background: transparent; }");
    qss += QStringLiteral("#homeGreeting { color: %1; font-size: 26px; font-weight: 800; }").arg(fg.name());
    qss += QStringLiteral("#homeGreetingSub, #cardMuted { color: %1; }").arg(muted.name());
    qss += QStringLiteral("#sectionTitle { color: %1; font-size: 16px; font-weight: 700; }").arg(fg.name());

    // Cards
    qss += QStringLiteral("QFrame[card=\"true\"] { background-color: %1; border: 1px solid %2; border-radius: 16px; }")
               .arg(rgba(card, 235), rgba(fg, 22));
    qss += QStringLiteral("QFrame#recentRow:hover { background-color: %1; }").arg(rgba(cardHover, 245));
    qss += QStringLiteral("QFrame#recentRow[running=\"true\"] { border: 1px solid %1; }").arg(accent.name());

    // The pinned XylarJava Client: gold to magenta, a little louder than everything else.
    const FeatureColors featured = featureColors(dark);
    const QColor gold = featured.gold;
    const QColor flame = featured.flame;
    const QColor magenta = featured.magenta;
    const QString featureGradient = QStringLiteral("qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 %1, stop: 0.5 %2, stop: 1 %3)")
                                        .arg(gold.name(), flame.name(), magenta.name());
    qss += QStringLiteral(
               "QFrame#featuredRow { border: 1px solid %1;"
               " background-color: qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 %2, stop: 0.6 %3, stop: 1 %4); }"
               "QFrame#featuredRow:hover { border-color: %5; }")
               .arg(rgba(flame, 150), rgba(gold, dark ? 38 : 46), rgba(card, 235), rgba(magenta, dark ? 30 : 26), rgba(gold, 220));
    qss += QStringLiteral("#featuredName { color: %1; font-size: 16px; font-weight: 900; background: transparent; }").arg(flame.name());
    qss += QStringLiteral(
               "#featuredPlayButton { border-image: none; margin: 0; background-color: %1; color: %2; border: none; border-radius: 10px;"
               " font-weight: 800; padding: 6px 16px; }"
               "#featuredPlayButton:hover { background-color: %3; }")
               .arg(featureGradient, textOn(flame).name(),
                    QStringLiteral("qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 %1, stop: 0.5 %2, stop: 1 %3)")
                        .arg(gold.lighter(110).name(), flame.lighter(110).name(), magenta.lighter(110).name()));
    qss += QStringLiteral("#cardTitle, #accountName, #recentName { color: %1; font-size: 15px; font-weight: 700; background: transparent; }")
               .arg(fg.name());
    qss += QStringLiteral("#accountName { font-size: 18px; }");
    qss += QStringLiteral("#accountFace, #recentIcon { background-color: %1; border-radius: 12px; }").arg(rgba(fg, 14));
    qss += QStringLiteral("QFrame[card=\"true\"] QLabel { background: transparent; border: none; }");
    qss += QStringLiteral("#statTile { background-color: %1; border: none; border-radius: 12px; }").arg(rgba(fg, 10));
    qss += QStringLiteral("#statValue { color: %1; font-size: 22px; font-weight: 800; }").arg(accent.name());

    // Buttons
    qss += QStringLiteral(
               "#cardButton, #iconButton { border-image: none; margin: 0; background-color: %1; color: %2; border: 1px solid %3; border-radius: 10px; padding: 8px 14px;"
               " font-weight: 600; }"
               "#cardButton:hover, #iconButton:hover { background-color: %4; border-color: %5; }")
               .arg(rgba(fg, 10), fg.name(), rgba(fg, 26), rgba(fg, 20), rgba(accent, 160));
    qss += QStringLiteral(
               "#accentButton { border-image: none; margin: 0; background-color: %1; color: %2; border: none; border-radius: 12px; padding: 8px 14px; font-weight: 800; }"
               "#accentButton:hover { background-color: %3; }")
               .arg(accent.name(), accentText.name(), accent.lighter(110).name());
    qss += QStringLiteral(
               "#rowPlayButton { border-image: none; margin: 0; background-color: %1; color: %2; border: none; border-radius: 10px; font-weight: 800; padding: 6px 16px; }"
               "#rowPlayButton:hover { background-color: %3; }"
               "#rowPlayButton:disabled { background-color: %4; color: %5; }")
               .arg(accent.name(), accentText.name(), accent.lighter(110).name(), rgba(fg, 18), muted.name());
    qss += QStringLiteral(
               "#stopButton { border-image: none; margin: 0; background-color: %1; color: #FFFFFF; border: none; border-radius: 10px; font-weight: 800; padding: 6px 16px; }"
               "#stopButton:hover { background-color: %2; }")
               .arg(danger.name(), danger.lighter(112).name());

    // Hero
    qss += QStringLiteral("#heroNameplate { color: #FFFFFF; background-color: rgba(0, 0, 0, 120); border-radius: 6px; padding: 3px 10px;"
                          " font-weight: 700; }");
    qss += QStringLiteral("#heroBar { background-color: %1; border: 1px solid %2; border-radius: 14px; }").arg(rgba(bg, 200), rgba(fg, 26));
    qss += QStringLiteral("#heroBar QLabel { background: transparent; }");
    qss += QStringLiteral("#heroIcon { background-color: %1; border-radius: 10px; }").arg(rgba(fg, 16));
    qss += QStringLiteral("#heroName { color: %1; font-size: 20px; font-weight: 800; }").arg(fg.name());
    qss += QStringLiteral("#heroDetails { color: %1; }").arg(muted.name());
    qss += QStringLiteral(
               "#heroPicker { border-image: none; margin: 0; color: %1; border: 1px solid %2; border-radius: 7px; padding: 2px 9px; background: transparent; }"
               "#heroPicker:hover { color: %3; border-color: %4; }"
               "#heroPicker::menu-indicator { image: none; width: 0; }")
               .arg(muted.name(), rgba(fg, 40), fg.name(), accent.name());
    qss += QStringLiteral(
               "#playButton { border-image: none; margin: 0; background-color: %1; color: %2; border: none; %3 font-size: 18px; font-weight: 800;"
               " letter-spacing: 1px; padding: 8px 40px; min-height: 34px; }"
               "#playButton:hover { background-color: %4; }"
               "#playButton:pressed { background-color: %5; }"
               "#playButton:disabled { background-color: %6; color: %7; }")
               .arg(playColor.name(), running ? QStringLiteral("#FFFFFF") : accentText.name(), playRadius, playColor.lighter(110).name(),
                    playColor.darker(112).name(), rgba(fg, 18), muted.name());
    qss += QStringLiteral(
               "#playMenuButton { border-image: none; margin: 0; background-color: %1; color: %2; border: none; border-left: 1px solid %3;"
               " border-top-right-radius: 12px; border-bottom-right-radius: 12px; border-top-left-radius: 0;"
               " border-bottom-left-radius: 0; padding: 0 12px; min-height: 50px; }"
               "#playMenuButton:hover { background-color: %4; }"
               "#playMenuButton:disabled { background-color: %5; }"
               "#playMenuButton::menu-indicator { image: none; width: 0; }")
               .arg(accent.name(), accentText.name(), splitColor.name(), accent.lighter(110).name(), rgba(fg, 18));

    if (clientSelected && !running) {
        // The client gets its own colours on the big button too.
        qss += QStringLiteral(
                   "#playButton { background-color: %1; color: %2; }"
                   "#playButton:hover { background-color: %3; }"
                   "#playMenuButton { background-color: %4; color: %5; border-left: 1px solid %6; }"
                   "#playMenuButton:hover { background-color: %7; }")
                   .arg(featureGradient, textOn(flame).name(),
                        QStringLiteral("qlineargradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 %1, stop: 1 %2)")
                            .arg(gold.lighter(108).name(), flame.lighter(108).name()),
                        magenta.name(), textOn(magenta).name(), magenta.darker(115).name(), magenta.lighter(110).name());
    }

    if (qss != m_appliedStyleSheet) {
        m_appliedStyleSheet = qss;
        setStyleSheet(qss);
    }
    m_updatingStyle = false;
}
