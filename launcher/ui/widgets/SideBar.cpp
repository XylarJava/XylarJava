#include "SideBar.h"

#include <QAction>
#include <QApplication>
#include <QEasingCurve>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "HomePage.h"

namespace {
QString rgba(const QColor& c, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

/// The theme's palette. The sidebar's own stylesheet pins its Window colour (and Qt skips widgets
/// with their own stylesheet when propagating palettes), so read the application palette instead.
QPalette themePalette(const QWidget* widget)
{
    return QApplication::palette(widget);
}
}  // namespace

SideBar::SideBar(QWidget* parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("sideBar"));
    setFixedWidth(84);
    setAttribute(Qt::WA_StyledBackground, true);

    // --- Logo ------------------------------------------------------------
    m_logo = new QLabel(this);
    m_logo->setObjectName(QStringLiteral("sideBarLogo"));
    m_logo->setFixedSize(42, 42);
    m_logo->setAlignment(Qt::AlignCenter);

    // The rail is icon-only; the name is kept for accessibility and the logo's tooltip.
    m_name = new QLabel(this);
    m_name->setObjectName(QStringLiteral("sideBarName"));
    m_name->hide();

    auto logoRow = new QHBoxLayout();
    logoRow->setContentsMargins(0, 0, 0, 0);
    logoRow->addWidget(m_logo, 0, Qt::AlignHCenter);

    // --- Sections --------------------------------------------------------
    m_pagesLayout = new QVBoxLayout();
    m_pagesLayout->setContentsMargins(0, 0, 0, 0);
    m_pagesLayout->setSpacing(8);
    m_pagesLayout->setAlignment(Qt::AlignHCenter);

    m_actionsLayout = new QVBoxLayout();
    m_actionsLayout->setContentsMargins(0, 0, 0, 0);
    m_actionsLayout->setSpacing(6);
    m_actionsLayout->setAlignment(Qt::AlignHCenter);

    m_bottomLayout = new QVBoxLayout();
    m_bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_bottomLayout->setSpacing(0);
    m_bottomLayout->setAlignment(Qt::AlignHCenter);

    auto divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("sideBarDivider"));
    divider->setFixedSize(32, 1);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 20, 0, 16);
    layout->setSpacing(0);
    layout->addLayout(logoRow);
    layout->addSpacing(28);
    layout->addLayout(m_pagesLayout);
    layout->addSpacing(16);
    layout->addWidget(divider, 0, Qt::AlignHCenter);
    layout->addSpacing(16);
    layout->addLayout(m_actionsLayout);
    layout->addStretch(1);
    layout->addLayout(m_bottomLayout);

    m_indicatorAnim = new QVariantAnimation(this);
    m_indicatorAnim->setDuration(260);
    m_indicatorAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_indicatorAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_indicator = value.toRectF();
        update();
    });

    updateStyle();
}

void SideBar::setLogo(const QIcon& logo, const QString& name)
{
    m_logo->setPixmap(logo.pixmap(42, 42));
    m_logo->setToolTip(name);
    m_name->setText(name);
}

QPushButton* SideBar::makeButton(const QString& objectName)
{
    auto button = new QPushButton(this);
    button->setObjectName(objectName);
    button->setFixedSize(52, 52);
    button->setIconSize(QSize(24, 24));
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

void SideBar::bindToAction(QPushButton* button, QAction* action, bool menuOpensUpwards)
{
    // QPushButton has no "default action", so mirror the action by hand.
    auto sync = [button, action] {
        // Icon-only rail: the label becomes the tooltip.
        QString label = action->iconText().isEmpty() ? action->text() : action->iconText();
        label.remove(QLatin1Char('&'));
        button->setText(QString());
        button->setIcon(action->icon());
        button->setToolTip(action->toolTip().isEmpty() ? label : action->toolTip());
        button->setAccessibleName(label);
        button->setEnabled(action->isEnabled());
        button->setVisible(action->isVisible());
    };
    sync();
    connect(action, &QAction::changed, button, sync);

    connect(button, &QPushButton::clicked, action, [button, action, menuOpensUpwards] {
        if (QMenu* menu = action->menu()) {
            const QSize menuSize = menu->sizeHint();
            const QPoint at = menuOpensUpwards ? button->mapToGlobal(QPoint(0, -menuSize.height() - 4))
                                               : button->mapToGlobal(QPoint(button->width() + 4, 0));
            menu->popup(at);
        } else {
            action->trigger();
        }
    });
}

int SideBar::addPage(const QIcon& icon, const QString& text)
{
    const int index = int(m_pageButtons.size());
    auto button = makeButton(QStringLiteral("sideNavButton"));
    button->setIcon(icon);
    button->setToolTip(text);
    button->setAccessibleName(text);
    button->setCheckable(true);
    connect(button, &QPushButton::clicked, this, [this, index] {
        setCurrentPage(index);
        emit pageRequested(index);
    });
    m_pageButtons.append(button);
    m_pagesLayout->addWidget(button);
    if (m_current < 0)
        setCurrentPage(index, false);
    return index;
}

void SideBar::setPageText(int index, const QString& text)
{
    if (index >= 0 && index < m_pageButtons.size()) {
        m_pageButtons.at(index)->setToolTip(text);
        m_pageButtons.at(index)->setAccessibleName(text);
    }
}

QPushButton* SideBar::addActionButton(QAction* action)
{
    auto button = makeButton(QStringLiteral("sideActionButton"));
    bindToAction(button, action, false);
    m_actionsLayout->addWidget(button);
    return button;
}

void SideBar::setAccountAction(QAction* action)
{
    if (m_accountAction == action)
        return;
    if (m_accountButton) {
        m_bottomLayout->removeWidget(m_accountButton);
        m_accountButton->deleteLater();
    }
    m_accountAction = action;
    m_accountButton = makeButton(QStringLiteral("sideAccountButton"));
    m_accountButton->setIconSize(QSize(34, 34));
    bindToAction(m_accountButton, action, true);
    m_bottomLayout->addWidget(m_accountButton);
}

void SideBar::setCurrentPage(int index, bool animate)
{
    if (index < 0 || index >= m_pageButtons.size())
        return;
    m_current = index;
    for (int i = 0; i < m_pageButtons.size(); i++)
        m_pageButtons.at(i)->setChecked(i == index);
    moveIndicator(animate);
}

void SideBar::moveIndicator(bool animate)
{
    if (m_current < 0 || m_current >= m_pageButtons.size())
        return;
    const QPushButton* button = m_pageButtons.at(m_current);
    const QRectF target(button->mapTo(this, QPoint(0, 0)), QSizeF(button->size()));

    if (!animate || !isVisible() || m_indicator.isNull()) {
        m_indicatorAnim->stop();
        m_indicator = target;
        update();
        return;
    }
    m_indicatorAnim->stop();
    m_indicatorAnim->setStartValue(m_indicator);
    m_indicatorAnim->setEndValue(target);
    m_indicatorAnim->start();
}

void SideBar::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    moveIndicator(false);
}

void SideBar::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    if (m_current < 0 || m_current >= m_pageButtons.size())
        return;

    // Layouts may not have placed the buttons yet on the first paints, so track
    // the active button whenever no slide is running.
    if (m_indicatorAnim->state() != QAbstractAnimation::Running) {
        const QPushButton* button = m_pageButtons.at(m_current);
        m_indicator = QRectF(button->mapTo(this, QPoint(0, 0)), QSizeF(button->size()));
    }
    if (m_indicator.isEmpty())
        return;

    const QColor accent = HomePage::accentFor(themePalette(this));
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor fill = accent;
    fill.setAlpha(44);
    QPainterPath pill;
    pill.addRoundedRect(m_indicator, 14, 14);
    painter.fillPath(pill, fill);
    QColor ring = accent;
    ring.setAlpha(90);
    painter.setPen(QPen(ring, 1));
    painter.drawPath(pill);

    // Accent tab hugging the rail's left edge, level with the active button.
    const QRectF bar(0, m_indicator.top() + 12, 4, m_indicator.height() - 24);
    QPainterPath barPath;
    barPath.addRoundedRect(bar.adjusted(-2, 0, 0, 0), 2, 2);
    painter.fillPath(barPath, accent);
}

void SideBar::changeEvent(QEvent* event)
{
    switch (event->type()) {
        case QEvent::PaletteChange:
        case QEvent::ApplicationPaletteChange:
        case QEvent::StyleChange:
        case QEvent::ParentChange:
            // Parents may get the new palette after us, so restyle once things have settled.
            if (!m_restylePending) {
                m_restylePending = true;
                QTimer::singleShot(0, this, [this] {
                    m_restylePending = false;
                    updateStyle();
                    update();
                });
            }
            break;
        default:
            break;
    }
    QFrame::changeEvent(event);
}

void SideBar::updateStyle()
{
    if (m_updatingStyle)
        return;
    m_updatingStyle = true;

    const QPalette pal = themePalette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = HomePage::accentFor(pal);
    const bool dark = bg.lightnessF() < 0.5f;
    const QColor side = dark ? bg.darker(118) : bg.darker(104);

    const QString qss = QStringLiteral(
                            "#sideBar { background-color: %1; border: none; border-right: 1px solid %2; }"
                            "#sideBarName { color: %3; font-size: 17px; font-weight: 800; background: transparent; }"
                            "#sideBarDivider { background-color: %2; }"
                            "#sideNavButton, #sideActionButton, #sideAccountButton {"
                            " color: %4; background: transparent; border: none; border-image: none; border-radius: 14px; padding: 0; margin: 0;"
                            " min-width: 52px; max-width: 52px; min-height: 52px; max-height: 52px; }"
                            "#sideNavButton:hover, #sideActionButton:hover, #sideAccountButton:hover { background-color: %5; color: %3; }"
                            "#sideNavButton:checked { color: %6; background: transparent; }"
                            "#sideActionButton:disabled { color: %2; }"
                            "#sideAccountButton { border: 1px solid %2; border-radius: 16px; }"
                            "#sideAccountButton:hover { border-color: %6; }")
                            .arg(side.name(), rgba(fg, 28), fg.name(), rgba(fg, 175), rgba(fg, 18), accent.name());

    if (qss != m_appliedStyleSheet) {
        m_appliedStyleSheet = qss;
        setStyleSheet(qss);
    }
    m_updatingStyle = false;
}
