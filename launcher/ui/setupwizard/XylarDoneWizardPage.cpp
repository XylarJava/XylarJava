// SPDX-License-Identifier: GPL-3.0-only
#include "XylarDoneWizardPage.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QVBoxLayout>

#include <BuildConfig.h>

XylarDoneWizardPage::XylarDoneWizardPage(QWidget* parent) : BaseWizardPage(parent)
{
    setObjectName(QStringLiteral("xylarDonePage"));
    setFinalPage(true);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(16);

    m_heading = new QLabel(this);
    m_heading->setObjectName(QStringLiteral("setupHeading"));
    m_lead = new QLabel(this);
    m_lead->setObjectName(QStringLiteral("setupLead"));
    m_lead->setWordWrap(true);
    layout->addWidget(m_heading);
    layout->addWidget(m_lead);
    layout->addSpacing(4);

    const char* icons[3] = { "new", "minecraft", "settings" };
    for (int i = 0; i < 3; i++) {
        auto card = new QFrame(this);
        card->setObjectName(QStringLiteral("setupCard"));
        auto badge = new QLabel(card);
        badge->setObjectName(QStringLiteral("setupBadge"));
        badge->setFixedSize(38, 38);
        badge->setAlignment(Qt::AlignCenter);
        badge->setPixmap(QIcon::fromTheme(QString::fromLatin1(icons[i])).pixmap(22, 22));
        m_tipTitles[i] = new QLabel(card);
        m_tipTitles[i]->setObjectName(QStringLiteral("setupCardTitle"));
        m_tipTexts[i] = new QLabel(card);
        m_tipTexts[i]->setObjectName(QStringLiteral("setupMuted"));
        m_tipTexts[i]->setWordWrap(true);
        auto text = new QVBoxLayout();
        text->setSpacing(3);
        text->addWidget(m_tipTitles[i]);
        text->addWidget(m_tipTexts[i]);
        auto row = new QHBoxLayout(card);
        row->setContentsMargins(14, 12, 14, 12);
        row->setSpacing(14);
        row->addWidget(badge, 0, Qt::AlignTop);
        row->addLayout(text, 1);
        layout->addWidget(card);
    }
    layout->addStretch(1);

    retranslate();
}

void XylarDoneWizardPage::retranslate()
{
    setTitle(QString());
    setProperty("stepName", tr("Done"));
    m_heading->setText(tr("You're all set"));
    m_lead->setText(tr("%1 is ready. Here's where to start.").arg(BuildConfig.LAUNCHER_DISPLAYNAME));
    m_tipTitles[0]->setText(tr("Add your first instance"));
    m_tipTexts[0]->setText(tr("Vanilla, Fabric, Forge or a modpack: Home > Add instance, pick a version and press Play."));
    m_tipTitles[1]->setText(tr("Try the XylarJava Client 1.8"));
    m_tipTexts[1]->setText(tr("It's pinned on the Home screen. In game, press Right Shift for the mods."));
    m_tipTitles[2]->setText(tr("Make it yours"));
    m_tipTexts[2]->setText(tr("Themes, icons, Java and Discord status are in Settings, always one click away in the sidebar."));
}
