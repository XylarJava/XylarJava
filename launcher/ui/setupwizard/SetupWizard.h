#pragma once

#include <QIcon>
#include <QWizard>

class BaseWizardPage;
class SetupStepsWidget;

/**
 * First-run setup. A step list on the left shows where you are (Welcome, Language, Java,
 * Appearance, Account, Done); the pages themselves are on the right.
 * Pages can set a "stepName" property to name themselves in the list when they have no title.
 */
class SetupWizard : public QWizard {
    Q_OBJECT

   public: /* con/destructors */
    explicit SetupWizard(QWidget* parent = 0);
    virtual ~SetupWizard();

    void changeEvent(QEvent* event) override;
    /// Logo shown at the top of the step list (and as the window icon).
    void setLogo(const QIcon& logo);
    BaseWizardPage* getBasePage(int id);
    BaseWizardPage* getCurrentBasePage();

   protected:
    void showEvent(QShowEvent* event) override;

   private slots:
    void pageChanged(int id);

   private: /* methods */
    void retranslate();
    void updateStyle();
    void adoptPageTitles();

    SetupStepsWidget* m_steps = nullptr;
    QString m_appliedStyleSheet;
    bool m_updatingStyle = false;
};
