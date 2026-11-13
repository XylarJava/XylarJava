#pragma once

#include <QWidget>

#include "BaseVersion.h"
#include "tasks/Task.h"
#include "ui/pages/BasePage.h"

namespace Ui {
class CustomPage;
}

class NewInstanceDialog;
class QCheckBox;
class QLabel;

class CustomPage : public QWidget, public BasePage {
    Q_OBJECT

   public:
    explicit CustomPage(NewInstanceDialog* dialog, QWidget* parent = 0);
    virtual ~CustomPage();
    virtual QString displayName() const override { return tr("Custom"); }
    virtual QIcon icon() const override { return QIcon::fromTheme("minecraft"); }
    virtual QString id() const override { return "vanilla"; }
    virtual QString helpPage() const override { return "Vanilla-platform"; }
    virtual bool shouldDisplay() const override;
    void retranslate() override;

    void openedImpl() override;

    BaseVersion::Ptr selectedVersion() const;
    BaseVersion::Ptr selectedLoaderVersion() const;
    QString selectedLoader() const;

   public slots:
    void setSelectedVersion(BaseVersion::Ptr version);
    void setSelectedLoaderVersion(BaseVersion::Ptr version);

   private slots:
    void filterChanged();
    void loaderFilterChanged();

   private:
    void refresh();
    void loaderRefresh();
    void suggestCurrent();

   private:
    QCheckBox* m_rustFilter = nullptr;
    QLabel* m_rustVersion = nullptr;
    bool initialized = false;
    NewInstanceDialog* dialog = nullptr;
    Ui::CustomPage* ui = nullptr;
    bool m_versionSetByUser = false;
    BaseVersion::Ptr m_selectedVersion;
    BaseVersion::Ptr m_selectedLoaderVersion;
    QString m_selectedLoader;
};
