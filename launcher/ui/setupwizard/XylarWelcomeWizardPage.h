// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "BaseWizardPage.h"

class QLabel;
class QLineEdit;
class QCheckBox;

/**
 * First-run welcome page for XylarJava.
 *
 * A short introduction (logo, what the launcher does), then an optional offline
 * account and the skin server. All fields are optional: leaving them untouched
 * keeps the current defaults.
 */
class XylarWelcomeWizardPage : public BaseWizardPage {
    Q_OBJECT

   public:
    explicit XylarWelcomeWizardPage(QWidget* parent = nullptr);
    ~XylarWelcomeWizardPage() override;

    bool validatePage() override;

   protected:
    void retranslate() override;

   private:
    QLabel* m_logoLabel = nullptr;
    QLabel* m_welcomeLabel = nullptr;
    QLabel* m_leadLabel = nullptr;
    QLabel* m_featureTitles[3] = {};
    QLabel* m_featureTexts[3] = {};
    QLabel* m_offlineTitle = nullptr;
    QLabel* m_offlineText = nullptr;
    QLabel* m_accountLabel = nullptr;
    QLineEdit* m_usernameEdit = nullptr;
    QLabel* m_backendLabel = nullptr;
    QLineEdit* m_backendEdit = nullptr;
    QCheckBox* m_enableSkinsCheck = nullptr;
    QLabel* m_hintLabel = nullptr;
};
