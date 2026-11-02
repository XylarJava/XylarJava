#pragma once

#include <QListView>
#include <QModelIndex>
#include <QWidget>

#include "ui/pages/BasePageContainer.h"
#include "ui/pages/BasePageProvider.h"

class QLayout;
class IconLabel;
class QSortFilterProxyModel;
class PageModel;
class QLabel;
class QListView;
class QLineEdit;
class QStackedLayout;
class QGridLayout;

class PageContainer : public QWidget, public BasePageContainer {
    Q_OBJECT
   public:
    explicit PageContainer(BasePageProvider* pageProvider, QString defaultId = QString(), QWidget* parent = 0);
    virtual ~PageContainer() {}

    void addButtons(QWidget* buttons);
    void addButtons(QLayout* buttons);

    void useSidebarStyle(bool sidebar);

    /*
     * Save any unsaved state and prepare to be closed.
     * @return true if everything can be saved, false if there is something that requires attention
     */
    bool prepareToClose();
    bool saveAll();

    /* request close - used by individual pages */
    bool requestClose() override
    {
        if (m_container) {
            return m_container->requestClose();
        }
        return false;
    }

    bool selectPage(QString pageId) override;
    BasePage* selectedPage() const override;
    BasePage* getPage(QString pageId) override;
    const QList<BasePage*>& getPages() const;

    void refreshContainer() override;
    virtual void setParentContainer(BasePageContainer* container) { m_container = container; };

    void changeEvent(QEvent*) override;

    void hidePageList() { m_pageList->hide(); }
    void setPageListVisible(bool visible) { m_pageList->setVisible(visible); }

   private:
    void createUI();
    void retranslate();

   public slots:
    void help();

   signals:
    /** Emitted when the currently selected page is changed */
    void selectedPageChanged(BasePage* previous, BasePage* selected);

   private slots:
    void currentChanged(const QModelIndex& current);
    void showPage(int row);

   private:
    BasePageContainer* m_container = nullptr;
    BasePage* m_currentPage = 0;
    QSortFilterProxyModel* m_proxyModel;
    PageModel* m_model;
    QStackedLayout* m_pageStack;
    QListView* m_pageList;
    QLabel* m_header;
    QGridLayout* m_layout;
};
