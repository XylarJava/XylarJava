// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "BaseWizardPage.h"

class QLabel;

/** Last page of the first-run setup: you're ready, plus a few things worth knowing. */
class XylarDoneWizardPage : public BaseWizardPage {
    Q_OBJECT

   public:
    explicit XylarDoneWizardPage(QWidget* parent = nullptr);

   protected:
    void retranslate() override;

   private:
    QLabel* m_heading = nullptr;
    QLabel* m_lead = nullptr;
    QLabel* m_tipTitles[3] = {};
    QLabel* m_tipTexts[3] = {};
};
