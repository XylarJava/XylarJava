// SPDX-License-Identifier: GPL-3.0-only
#include "XylarWelcomeWizardPage.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

#include <Application.h>
#include <BuildConfig.h>
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/AuthFlow.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "settings/SettingsObject.h"

XylarWelcomeWizardPage::XylarWelcomeWizardPage(QWidget* parent) : BaseWizardPage(parent)
{
    setObjectName(QStringLiteral("xylarWelcomePage"));

    auto layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    // Branding: the application logo, centered.
    m_logoLabel = new QLabel(this);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    {
        QIcon logo = APPLICATION->logo();
        if (!logo.isNull()) {
            m_logoLabel->setPixmap(logo.pixmap(QSize(96, 96)));
        }
    }
    layout->addWidget(m_logoLabel);

    m_welcomeLabel = new QLabel(this);
    m_welcomeLabel->setWordWrap(true);
    m_welcomeLabel->setAlignment(Qt::AlignCenter);
    m_welcomeLabel->setTextFormat(Qt::RichText);
    layout->addWidget(m_welcomeLabel);

    layout->addSpacing(8);

    // Offline account + skin backend fields.
    auto form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(tr("3–16 letters, numbers or underscores"));
    m_usernameEdit->setMaxLength(16);
    // Only valid Minecraft offline usernames (matches the skin backend rules).
    auto validator = new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9_]{0,16}"), this);
    m_usernameEdit->setValidator(validator);
    m_accountLabel = new QLabel(this);
    form->addRow(m_accountLabel, m_usernameEdit);

    m_backendEdit = new QLineEdit(this);
    m_backendEdit->setText(APPLICATION->settings()->get("XylarSkinBackendUrl").toString());
    m_backendLabel = new QLabel(this);
    form->addRow(m_backendLabel, m_backendEdit);

    layout->addLayout(form);

    m_enableSkinsCheck = new QCheckBox(this);
    m_enableSkinsCheck->setChecked(true);
    layout->addWidget(m_enableSkinsCheck);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setEnabled(false);  // de-emphasised helper text
    layout->addWidget(m_hintLabel);

    layout->addStretch(1);

    retranslate();
}

XylarWelcomeWizardPage::~XylarWelcomeWizardPage() = default;

bool XylarWelcomeWizardPage::validatePage()
{
    auto settings = APPLICATION->settings();

    // Persist the skin backend URL (empty disables remote skins but keeps cache).
    QString backend = m_backendEdit->text().trimmed();
    while (backend.endsWith('/')) {
        backend.chop(1);
    }
    if (!m_enableSkinsCheck->isChecked()) {
        backend.clear();
    }
    settings->set("XylarSkinBackendUrl", backend);

    // Optionally create an offline account from the chosen username.
    QString username = m_usernameEdit->text().trimmed();
    if (username.length() >= 3) {
        auto accounts = APPLICATION->accounts();
        if (MinecraftAccountPtr account = MinecraftAccount::createOffline(username)) {
            account->login()->start();
            accounts->addAccount(account);
            if (accounts->count() == 1) {
                accounts->setDefaultAccount(account);
            }
        }
    }

    return true;
}

void XylarWelcomeWizardPage::retranslate()
{
    const QString name = BuildConfig.LAUNCHER_DISPLAYNAME;
    setTitle(tr("Welcome to %1").arg(name));
    setSubTitle(tr("Let's get you set up to play."));

    m_welcomeLabel->setText(tr("<h2>%1</h2>"
                               "<p>A fast, offline-friendly Minecraft launcher with its own skin system.</p>")
                                .arg(name));

    m_accountLabel->setText(tr("Offline username"));
    m_backendLabel->setText(tr("Skin backend URL"));
    m_enableSkinsCheck->setText(tr("Enable online skins (recommended)"));
    m_hintLabel->setText(tr("Pick a username to create an offline account now — you can add more later. "
                            "The skin backend lets your skin show up for other %1 players; leave the default "
                            "unless you host your own.")
                             .arg(name));
}
