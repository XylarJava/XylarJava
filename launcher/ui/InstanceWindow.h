#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QToolButton>

#include "LaunchController.h"
#include "launch/LaunchTask.h"

#include "ui/pages/BasePageContainer.h"

#include "QObjectPtr.h"

class QPushButton;
class PageContainer;
class InstanceWindow : public QMainWindow, public BasePageContainer {
    Q_OBJECT

   public:
    explicit InstanceWindow(BaseInstance* proc, QWidget* parent = 0);
    virtual ~InstanceWindow() = default;

    bool selectPage(QString pageId) override;
    BasePage* selectedPage() const override;
    void refreshContainer() override;

    QString instanceId();

    /// Just the game log, without the list of instance pages (used when a game starts or crashes).
    void setConsoleOnly(bool consoleOnly);
    bool isConsoleOnly() const { return m_consoleOnly; }

    // save all settings and changes (prepare for launch)
    bool saveAll();

    // request closing the window (from a page)
    bool requestClose() override;

   signals:
    void isClosing();

   private slots:
    void instanceLaunchTaskChanged(LaunchTask* proc);
    void runningStateChanged(bool running);
    void on_instanceStatusChanged(BaseInstance::Status, BaseInstance::Status newStatus);

   protected:
    void closeEvent(QCloseEvent*) override;

   private:
    void updateButtons();

   private:
    LaunchTask* m_proc;
    BaseInstance* m_instance;
    bool m_doNotSave = false;
    bool m_consoleOnly = false;
    PageContainer* m_container = nullptr;
    QPushButton* m_closeButton = nullptr;
    QToolButton* m_launchButton = nullptr;
    QPushButton* m_killButton = nullptr;
};
