#include "SideBar.h"

#include <QAction>
#include <QEasingCurve>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVariantAnimation>

#include "HomePage.h"

namespace {
QString rgba(const QColor& c, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}
}  // namespace

SideBar::SideBar(QWidget* parent) : QFrame(parent)
{
    setObjectName(QStringLiteral("sideBar"));
    setFixedWidth(212);
    setAttribute(Qt::WA_StyledBackground, true);

    // --- Logo ------------------------------------------------------------
    m_logo = new QLabel(this);
    m_logo->setObjectName(QStringLiteral("sideBarLogo"));
    m_logo->setFixedSize(34, 34);

    m_name = new QLabel(this);
    m_name->setObjectName(QStringLiteral("sideBarName"));

    auto logoRow = new QHBoxLayout();
    logoRow->setContentsMargins(6, 0, 0, 0);
    logoRow->setSpacing(10);
    logoRow->addWidget(m_logo);
    logoRow->addWidget(m_name, 1);

    // --- Sections --------------------------------------------------------
    m_pagesLayout = new QVBoxLayout();
    m_pagesLayout->setContentsMargins(0, 0, 0, 0);
    m_pagesLayout->setSpacing(4);

    m_actionsLayout = new QVBoxLayout();
    m_actionsLayout->setContentsMargins(0, 0, 0, 0);
    m_actionsLayout->setSpacing(2);

    m_bottomLayout = new QVBoxLayout();
    m_bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_bottomLayout->setSpacing(0);

    auto divider = new QFrame(this);
    divider->setObjectName(QStringLiteral("sideBarDivider"));
    divider->setFixedHeight(1);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 18, 14, 14);
    layout->setSpacing(0);
    layout->addLayout(logoRow);
    layout->addSpacing(26);
    layout->addLayout(m_pagesLayout);
    layout->addSpacing(14);
    layout->addWidget(divider);
    layout->addSpacing(14);
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
    m_logo->setPixmap(logo.pixmap(34, 34));
    m_name->setText(name);
}

QPushButton* SideBar::makeButton(const QString& objectName)
{
    auto button = new QPushButton(this);
    button->setObjectName(objectName);
    button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    button->setIconSize(QSize(20, 20));
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setFocusPolicy(Qt::TabFocus);
    return button;
}

void SideBar::bindToAction(QPushButton* button, QAction* action, bool menuOpensUpwards)
{
    // QPushButton has no "default action", so mirror the action by hand.
    auto sync = [button, action] {
        button->setText(action->iconText().isEmpty() ? action->text() : action->iconText());
        button->setIcon(action->icon());
        button->setToolTip(action->toolTip());
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
    button->setText(text);
    button->setCheckable(true);
    button->setMinimumHeight(40);
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
    if (index >= 0 && index < m_pageButtons.size())
        m_pageButtons.at(index)->setText(text);
}

QPushButton* SideBar::addActionButton(QAction* action)
{
    auto button = makeButton(QStringLiteral("sideActionButton"));
    button->setMinimumHeight(36);
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
    m_accountButton->setIconSize(QSize(28, 28));
    m_accountButton->setMinimumHeight(52);
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

    const QColor accent = HomePage::accentFor(palette());
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QColor fill = accent;
    fill.setAlpha(38);
    QPainterPath pill;
    pill.addRoundedRect(m_indicator, 10, 10);
    painter.fillPath(pill, fill);

    // Small accent bar on the left edge of the pill.
    const QRectF bar(m_indicator.left(), m_indicator.top() + 10, 3, m_indicator.height() - 20);
    QPainterPath barPath;
    barPath.addRoundedRect(bar, 1.5, 1.5);
    painter.fillPath(barPath, accent);
}

void SideBar::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        updateStyle();
        update();
    }
    QFrame::changeEvent(event);
}

void SideBar::updateStyle()
{
    if (m_updatingStyle)
        return;
    m_updatingStyle = true;

    const QPalette pal = palette();
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
                            " color: %4; background: transparent; border: none; border-radius: 10px;"
                            " padding: 6px 12px; text-align: left; }"
                            "#sideNavButton { font-weight: 600; padding-left: 16px; }"
                            "#sideNavButton:hover, #sideActionButton:hover, #sideAccountButton:hover { background-color: %5; color: %3; }"
                            "#sideNavButton:checked { color: %6; background: transparent; }"
                            "#sideActionButton:disabled { color: %2; }"
                            "#sideAccountButton { border: 1px solid %2; padding: 6px 10px; font-weight: 600; color: %3; }")
                            .arg(side.name(), rgba(fg, 28), fg.name(), rgba(fg, 175), rgba(fg, 18), accent.name());

    if (qss != m_appliedStyleSheet) {
        m_appliedStyleSheet = qss;
        setStyleSheet(qss);
    }
    m_updatingStyle = false;
}
