#pragma once

#include <QFrame>
#include <QIcon>
#include <QList>

class QAction;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QVariantAnimation;

/**
 * Vertical navigation for the main window: the XylarJava logo at the top,
 * page buttons (Home, Instances, ...), quick actions, and the account
 * switcher pinned to the bottom.
 *
 * The highlight behind the active page slides to its new position when the
 * page changes. Colours come from the active palette.
 */
class SideBar : public QFrame {
    Q_OBJECT

   public:
    explicit SideBar(QWidget* parent = nullptr);

    void setLogo(const QIcon& logo, const QString& name);

    /// Adds a navigation entry and returns its page index.
    int addPage(const QIcon& icon, const QString& text);
    void setPageText(int index, const QString& text);

    /// Adds a button bound to an existing action (text, icon, menu and visibility follow the action).
    QPushButton* addActionButton(QAction* action);

    /// Adds the action that shows the active account, pinned to the bottom.
    void setAccountAction(QAction* action);

    void setCurrentPage(int index, bool animate = true);
    int currentPage() const { return m_current; }

   signals:
    void pageRequested(int index);

   protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

   private:
    void updateStyle();
    void moveIndicator(bool animate);
    QPushButton* makeButton(const QString& objectName);
    void bindToAction(QPushButton* button, QAction* action, bool menuOpensUpwards);

    QLabel* m_logo = nullptr;
    QLabel* m_name = nullptr;
    QVBoxLayout* m_pagesLayout = nullptr;
    QVBoxLayout* m_actionsLayout = nullptr;
    QVBoxLayout* m_bottomLayout = nullptr;
    QList<QPushButton*> m_pageButtons;
    QPushButton* m_accountButton = nullptr;
    QAction* m_accountAction = nullptr;

    int m_current = -1;
    QRectF m_indicator;
    QVariantAnimation* m_indicatorAnim = nullptr;

    bool m_updatingStyle = false;
    bool m_restylePending = false;
    QString m_appliedStyleSheet;
};
