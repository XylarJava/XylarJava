// SPDX-License-Identifier: GPL-3.0-only
#include "XylarWelcomeWizardPage.h"
#include "ui/dialogs/AccountIdentity.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
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
    setProperty("stepName", tr("Welcome"));

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(18);

    // Logo, heading and one line about the launcher
    m_logoLabel = new QLabel(this);
    m_logoLabel->setFixedSize(76, 76);
    {
        QIcon logo = APPLICATION->logo();
        if (!logo.isNull())
            m_logoLabel->setPixmap(logo.pixmap(QSize(76, 76)));
    }
    m_welcomeLabel = new QLabel(this);
    m_welcomeLabel->setObjectName(QStringLiteral("setupHeading"));
    m_leadLabel = new QLabel(this);
    m_leadLabel->setObjectName(QStringLiteral("setupLead"));
    m_leadLabel->setWordWrap(true);
    auto heading = new QVBoxLayout();
    heading->setSpacing(4);
    heading->addStretch(1);
    heading->addWidget(m_welcomeLabel);
    heading->addWidget(m_leadLabel);
    heading->addStretch(1);
    auto top = new QHBoxLayout();
    top->setSpacing(18);
    top->addWidget(m_logoLabel, 0, Qt::AlignTop);
    top->addLayout(heading, 1);
    layout->addLayout(top);

    // Three things worth knowing
    auto features = new QHBoxLayout();
    features->setSpacing(12);
    const char* icons[3] = { "launch", "accounts", "minecraft" };
    for (int i = 0; i < 3; i++) {
        auto card = new QFrame(this);
        card->setObjectName(QStringLiteral("setupCard"));
        auto badge = new QLabel(card);
        badge->setObjectName(QStringLiteral("setupBadge"));
        badge->setFixedSize(34, 34);
        badge->setAlignment(Qt::AlignCenter);
        badge->setPixmap(QIcon::fromTheme(QString::fromLatin1(icons[i])).pixmap(20, 20));
        m_featureTitles[i] = new QLabel(card);
        m_featureTitles[i]->setObjectName(QStringLiteral("setupCardTitle"));
        m_featureTexts[i] = new QLabel(card);
        m_featureTexts[i]->setObjectName(QStringLiteral("setupMuted"));
        m_featureTexts[i]->setWordWrap(true);
        auto cardLayout = new QVBoxLayout(card);
        cardLayout->setContentsMargins(14, 14, 14, 14);
        cardLayout->setSpacing(6);
        cardLayout->addWidget(badge);
        cardLayout->addSpacing(2);
        cardLayout->addWidget(m_featureTitles[i]);
        cardLayout->addWidget(m_featureTexts[i]);
        cardLayout->addStretch(1);
        features->addWidget(card, 1);
    }
    layout->addLayout(features);

    // Optional offline account and the skin server
    auto offline = new QFrame(this);
    offline->setObjectName(QStringLiteral("setupCard"));
    auto offlineLayout = new QVBoxLayout(offline);
    offlineLayout->setContentsMargins(16, 14, 16, 14);
    offlineLayout->setSpacing(10);
    m_offlineTitle = new QLabel(offline);
    m_offlineTitle->setObjectName(QStringLiteral("setupCardTitle"));
    m_offlineText = new QLabel(offline);
    m_offlineText->setObjectName(QStringLiteral("setupMuted"));
    m_offlineText->setWordWrap(true);
    offlineLayout->addWidget(m_offlineTitle);
    offlineLayout->addWidget(m_offlineText);

    auto form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignLeft);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(8);

    m_usernameEdit = new QLineEdit(offline);
    m_usernameEdit->setMaxLength(16);
    // Only valid Minecraft offline usernames (matches the skin backend rules).
    auto validator = new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9_]{0,16}"), this);
    m_usernameEdit->setValidator(validator);
    m_accountLabel = new QLabel(offline);
    form->addRow(m_accountLabel, m_usernameEdit);

    m_backendEdit = new QLineEdit(offline);
    m_backendEdit->setText(APPLICATION->settings()->get("XylarSkinBackendUrl").toString());
    m_backendLabel = new QLabel(offline);
    form->addRow(m_backendLabel, m_backendEdit);
    offlineLayout->addLayout(form);

    m_enableSkinsCheck = new QCheckBox(offline);
    m_enableSkinsCheck->setChecked(true);
    offlineLayout->addWidget(m_enableSkinsCheck);
    layout->addWidget(offline);

    m_hintLabel = new QLabel(this);
    m_hintLabel->setObjectName(QStringLiteral("setupMuted"));
    m_hintLabel->setWordWrap(true);
    m_hintLabel->setTextFormat(Qt::PlainText);
    layout->addWidget(m_hintLabel);

    layout->addStretch(1);

    retranslate();
}

XylarWelcomeWizardPage::~XylarWelcomeWizardPage() = default;

bool XylarWelcomeWizardPage::validatePage()
{
    auto settings = APPLICATION->settings();

    // Identity protection uses the service even when game skin rendering is disabled.
    QString backend = m_backendEdit->text().trimmed();
    while (backend.endsWith('/')) {
        backend.chop(1);
    }
    settings->set("XylarEnableSkins", m_enableSkinsCheck->isChecked());
    settings->set("XylarSkinBackendUrl", backend);

    // Optionally create an offline account from the chosen username.
    QString username = m_usernameEdit->text().trimmed();
    if (!username.isEmpty() && username.length() < 3) {
        m_hintLabel->setText(tr("Choose a username with 3 to 16 letters, numbers or underscores."));
        m_usernameEdit->setFocus();
        return false;
    }
    if (username.length() >= 3) {
        auto accounts = APPLICATION->accounts();
        if (accounts->getAccountByProfileName(username))
            return true;
        if (MinecraftAccountPtr account = MinecraftAccount::createOffline(username)) {
            QString error;
            if (!AccountIdentity::verify(this, account, &error)) {
                m_hintLabel->setText(error.isEmpty() ? tr("No account was created. Choose another username or try again.") : error);
                m_usernameEdit->setFocus();
                m_usernameEdit->selectAll();
                return false;
            }
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
    // No page title: the heading below is the title, and the step list names the page.
    setTitle(QString());
    setProperty("stepName", tr("Welcome"));

    m_welcomeLabel->setText(tr("Welcome to %1").arg(name));
    m_leadLabel->setText(tr("A fast Minecraft launcher with your skin, all your instances and our own 1.8.9 client. "
                            "Two minutes and you're playing."));

    m_featureTitles[0]->setText(tr("Fast and light"));
    m_featureTexts[0]->setText(tr("Starts quickly, stays out of the way and keeps itself up to date."));
    m_featureTitles[1]->setText(tr("Your skin, everywhere"));
    m_featureTexts[1]->setText(tr("Microsoft or offline: your real skin shows up on the Home screen and in game."));
    m_featureTitles[2]->setText(tr("XylarJava Client 1.8"));
    m_featureTexts[2]->setText(tr("A PvP client for 1.8.9 with 92 mods, XylarJava capes and an FPS boost preset."));

    m_offlineTitle->setText(tr("Play offline right away (optional)"));
    m_offlineText->setText(tr("Pick a username to create an offline account now. You can sign in with Microsoft on the next steps "
                              "or add more accounts later."));
    m_usernameEdit->setPlaceholderText(tr("3 to 16 letters, numbers or _"));
    m_accountLabel->setText(tr("Username"));
    m_backendLabel->setText(tr("Profile and skin server"));
    m_enableSkinsCheck->setText(tr("Enable Xylar skin rendering in my game"));
    m_hintLabel->setText(
        tr("Your username is reserved on this service, even when skin rendering is off. Leave the server unchanged unless you host your "
           "own."));
}
