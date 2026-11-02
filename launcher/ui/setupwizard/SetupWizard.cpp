#include "SetupWizard.h"

#include <QAbstractButton>
#include <QBoxLayout>
#include <QLabel>
#include <QApplication>
#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>

#include <BuildConfig.h>
#include "BaseWizardPage.h"
#include "ui/widgets/HomePage.h"

namespace {
QString rgba(const QColor& c, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

QColor mixColors(const QColor& a, const QColor& b, qreal t)
{
    const float f = float(qBound<qreal>(0.0, t, 1.0));
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f,
                            a.blueF() + (b.blueF() - a.blueF()) * f, 1.0f);
}
}  // namespace

/// The step list on the left of the wizard: logo, the steps with numbers or checks, and a hint.
class SetupStepsWidget : public QWidget {
   public:
    explicit SetupStepsWidget(QWizard* wizard) : QWidget(wizard), m_wizard(wizard)
    {
        setObjectName(QStringLiteral("setupSteps"));
        setFixedWidth(230);
        setMinimumHeight(420);
    }

    void refresh() { update(); }

    QIcon logo;

   protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QPalette pal = QApplication::palette(this);
        const QColor bg = pal.color(QPalette::Window);
        const QColor fg = pal.color(QPalette::WindowText);
        const QColor accent = HomePage::accentFor(pal);
        const bool dark = bg.lightnessF() < 0.5f;
        const QColor side = dark ? bg.darker(118) : bg.darker(104);
        const QColor muted = mixColors(fg, side, 0.45);

        QPainterPath panel;
        panel.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 16, 16);
        p.fillPath(panel, side);
        QColor border = fg;
        border.setAlpha(26);
        p.setPen(QPen(border, 1));
        p.drawPath(panel);

        // Logo and name
        const int pad = 22;
        if (!logo.isNull())
            p.drawPixmap(pad, 26, logo.pixmap(44, 44));
        QFont nameFont = font();
        nameFont.setPixelSize(17);
        nameFont.setWeight(QFont::ExtraBold);
        p.setFont(nameFont);
        p.setPen(fg);
        p.drawText(QRect(pad + 56, 26, width() - pad - 60, 24), Qt::AlignLeft | Qt::AlignVCenter, BuildConfig.LAUNCHER_DISPLAYNAME);
        QFont small = font();
        small.setPixelSize(12);
        p.setFont(small);
        p.setPen(muted);
        p.drawText(QRect(pad + 56, 48, width() - pad - 60, 20), Qt::AlignLeft | Qt::AlignVCenter, QObject::tr("Quick setup"));

        // Steps
        const QList<int> ids = m_wizard->pageIds();
        const int current = ids.indexOf(m_wizard->currentId());
        int y = 108;
        const int step = 52;
        for (int i = 0; i < ids.size(); i++) {
            QWizardPage* page = m_wizard->page(ids.at(i));
            QString name = page->property("stepName").toString();
            if (name.isEmpty())
                name = page->title();
            const bool done = i < current;
            const bool active = i == current;
            const QPointF c(pad + 13, y + 13);

            if (i + 1 < ids.size()) {
                QColor line = done ? accent : border;
                p.setPen(QPen(line, 2));
                p.drawLine(QPointF(c.x(), c.y() + 15), QPointF(c.x(), c.y() + step - 15));
            }

            if (done || active) {
                p.setPen(Qt::NoPen);
                p.setBrush(done ? accent : QColor(accent.red(), accent.green(), accent.blue(), 40));
                p.drawEllipse(c, 13, 13);
                if (active) {
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(accent, 2));
                    p.drawEllipse(c, 12, 12);
                }
            } else {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(border.lighter(160), 2));
                p.drawEllipse(c, 12, 12);
            }

            if (done) {
                QPainterPath check;
                check.moveTo(c.x() - 5.5, c.y() + 0.5);
                check.lineTo(c.x() - 1.5, c.y() + 4.5);
                check.lineTo(c.x() + 6, c.y() - 4);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(HomePage::textOn(accent), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPath(check);
            } else {
                QFont numberFont = font();
                numberFont.setPixelSize(12);
                numberFont.setBold(true);
                p.setFont(numberFont);
                p.setPen(active ? accent : muted);
                p.drawText(QRectF(c.x() - 13, c.y() - 13, 26, 26), Qt::AlignCenter, QString::number(i + 1));
            }

            QFont label = font();
            label.setPixelSize(14);
            label.setWeight(active ? QFont::Bold : QFont::Medium);
            p.setFont(label);
            p.setPen(active ? fg : (done ? mixColors(fg, side, 0.2) : muted));
            p.drawText(QRect(pad + 38, y, width() - pad - 44, 26), Qt::AlignLeft | Qt::AlignVCenter, name);
            y += step;
        }

        // Hint at the bottom
        p.setFont(small);
        p.setPen(muted);
        p.drawText(QRect(pad, height() - 70, width() - pad * 2, 50), Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap,
                   QObject::tr("You can change all of this later in Settings."));
    }

   private:
    QWizard* m_wizard;
};

SetupWizard::SetupWizard(QWidget* parent) : QWizard(parent)
{
    setObjectName(QStringLiteral("SetupWizard"));
    resize(940, 640);
    setMinimumSize(760, 560);
    setWizardStyle(QWizard::ClassicStyle);
    setOptions(QWizard::NoCancelButton | QWizard::IndependentPages | QWizard::HaveCustomButton1 | QWizard::NoBackButtonOnStartPage);

    m_steps = new SetupStepsWidget(this);
    setSideWidget(m_steps);

    retranslate();
    updateStyle();

    connect(this, &QWizard::currentIdChanged, this, &SetupWizard::pageChanged);
    // Connected once: pages that want it get a Refresh button (e.g. the Java page).
    connect(this, &QWizard::customButtonClicked, this, [this](int which) {
        if (which != QWizard::CustomButton1)
            return;
        if (auto page = getCurrentBasePage())
            page->refresh();
    });
}

void SetupWizard::retranslate()
{
    setButtonText(QWizard::NextButton, tr("Continue"));
    setButtonText(QWizard::BackButton, tr("Back"));
    setButtonText(QWizard::FinishButton, tr("Start playing"));
    setButtonText(QWizard::CustomButton1, tr("Refresh"));
    setWindowTitle(tr("Set up %1").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
}

void SetupWizard::updateStyle()
{
    if (m_updatingStyle)
        return;
    m_updatingStyle = true;

    const QPalette pal = QApplication::palette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = HomePage::accentFor(pal);
    const QColor accentText = HomePage::textOn(accent);
    const bool dark = bg.lightnessF() < 0.5f;
    const QColor card = dark ? bg.lighter(118) : bg.darker(104);
    const QColor muted = mixColors(fg, bg, 0.42);

    // Name the buttons so they can be styled; Qt gives them internal names otherwise.
    if (auto next = button(QWizard::NextButton))
        next->setObjectName(QStringLiteral("setupPrimary"));
    if (auto finish = button(QWizard::FinishButton))
        finish->setObjectName(QStringLiteral("setupPrimary"));
    if (auto back = button(QWizard::BackButton))
        back->setObjectName(QStringLiteral("setupSecondary"));
    if (auto refresh = button(QWizard::CustomButton1))
        refresh->setObjectName(QStringLiteral("setupSecondary"));

    QString qss;
    qss += QStringLiteral("QWizard#SetupWizard { background-color: %1; }").arg(bg.name());
    qss += QStringLiteral(
               "QPushButton#setupPrimary { background-color: %1; color: %2; border: none; border-radius: 10px; padding: 9px 26px;"
               " font-weight: 800; min-width: 110px; }"
               "QPushButton#setupPrimary:hover { background-color: %3; }"
               "QPushButton#setupPrimary:disabled { background-color: %4; color: %5; }")
               .arg(accent.name(), accentText.name(), accent.lighter(110).name(), rgba(fg, 18), muted.name());
    qss += QStringLiteral(
               "QPushButton#setupSecondary { background-color: %1; color: %2; border: 1px solid %3; border-radius: 10px;"
               " padding: 9px 20px; font-weight: 600; }"
               "QPushButton#setupSecondary:hover { background-color: %4; border-color: %5; }")
               .arg(rgba(fg, 10), fg.name(), rgba(fg, 26), rgba(fg, 20), rgba(accent, 160));
    // Shared pieces the Xylar pages use
    qss += QStringLiteral("#setupHeading { font-size: 26px; font-weight: 800; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#setupLead, #setupPageLead { font-size: 14px; color: %1; }").arg(muted.name());
    qss += QStringLiteral("#setupPageHeading { font-size: 22px; font-weight: 800; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#setupMuted { color: %1; }").arg(muted.name());
    qss += QStringLiteral("QFrame#setupCard { background-color: %1; border: 1px solid %2; border-radius: 14px; }").arg(card.name(), rgba(fg, 22));
    qss += QStringLiteral("QFrame#setupCard QLabel { background: transparent; border: none; }");
    qss += QStringLiteral("#setupCardTitle { font-size: 14px; font-weight: 700; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#setupBadge { background-color: %1; border-radius: 10px; }").arg(rgba(accent, 40));

    if (qss != m_appliedStyleSheet) {
        m_appliedStyleSheet = qss;
        setStyleSheet(qss);
    }
    m_updatingStyle = false;
}

BaseWizardPage* SetupWizard::getBasePage(int id)
{
    if (id == -1)
        return nullptr;
    auto pagePtr = page(id);
    if (!pagePtr)
        return nullptr;
    return dynamic_cast<BaseWizardPage*>(pagePtr);
}

BaseWizardPage* SetupWizard::getCurrentBasePage()
{
    return getBasePage(currentId());
}

void SetupWizard::adoptPageTitles()
{
    // Pages that use QWizard titles get our own heading inside the page instead, so every
    // page looks the same and the step list on the left stays at the top.
    for (int id : pageIds()) {
        QWizardPage* p = page(id);
        const QString title = p->title();
        if (title.isEmpty())
            continue;
        const QString subTitle = p->subTitle();
        p->setProperty("stepName", title);

        auto heading = p->findChild<QLabel*>(QStringLiteral("setupPageHeading"), Qt::FindDirectChildrenOnly);
        auto lead = p->findChild<QLabel*>(QStringLiteral("setupPageLead"), Qt::FindDirectChildrenOnly);
        if (!heading) {
            auto box = qobject_cast<QBoxLayout*>(p->layout());
            if (!box)
                continue;  // unusual layout: keep the QWizard title
            heading = new QLabel(p);
            heading->setObjectName(QStringLiteral("setupPageHeading"));
            lead = new QLabel(p);
            lead->setObjectName(QStringLiteral("setupPageLead"));
            lead->setWordWrap(true);
            box->insertWidget(0, heading);
            box->insertWidget(1, lead);
            box->insertSpacing(2, 10);
        }
        heading->setText(title);
        if (lead) {
            lead->setText(subTitle);
            lead->setVisible(!subTitle.isEmpty());
        }
        p->setTitle(QString());
        p->setSubTitle(QString());
    }
}

void SetupWizard::showEvent(QShowEvent* event)
{
    adoptPageTitles();
    QWizard::showEvent(event);
    updateStyle();
    m_steps->refresh();
}

void SetupWizard::pageChanged(int id)
{
    adoptPageTitles();
    m_steps->refresh();
    updateStyle();
    auto basePagePtr = getBasePage(id);
    if (!basePagePtr) {
        return;
    }
    if (basePagePtr->wantsRefreshButton()) {
        setButtonLayout({ QWizard::CustomButton1, QWizard::Stretch, QWizard::BackButton, QWizard::NextButton, QWizard::FinishButton });
    } else {
        setButtonLayout({ QWizard::Stretch, QWizard::BackButton, QWizard::NextButton, QWizard::FinishButton });
    }
}

void SetupWizard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        // Pages put their QWizard titles back when retranslated; move them into the page again.
        QTimer::singleShot(0, this, [this] {
            adoptPageTitles();
            m_steps->refresh();
        });
    } else if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        updateStyle();
        if (m_steps)
            m_steps->refresh();
    }
    QWizard::changeEvent(event);
}

void SetupWizard::setLogo(const QIcon& logo)
{
    setWindowIcon(logo);
    m_steps->logo = logo;
    m_steps->refresh();
}

SetupWizard::~SetupWizard() {}
