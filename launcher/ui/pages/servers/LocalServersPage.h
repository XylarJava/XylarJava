#pragma once

#include <QList>
#include <QPair>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include <functional>

class LocalServer;
struct LocalServerConfig;
class LocalServerList;
class QFrame;
class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QToolButton;

/**
 * "Servers" in the sidebar: make a Minecraft server that runs on this computer, start and stop it,
 * see how friends join (same Wi-Fi or over the internet), add mods or plugins, and use its console.
 */
class LocalServersPage : public QWidget {
    Q_OBJECT
   public:
    LocalServersPage(LocalServerList* servers, QNetworkAccessManager* network, QWidget* parent = nullptr);

    /// Instances to copy mods from: (name, mods folder). Set by the main window.
    void setModSources(std::function<QList<QPair<QString, QString>>()> sources) { m_modSources = std::move(sources); }
    /// Opens a folder in the file manager. Set by the main window.
    void setFolderOpener(std::function<void(const QString&)> opener) { m_openFolder = std::move(opener); }

    void select(LocalServer* server);

   protected:
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;

   private:
    /// "Create server": links playit.gg first when it isn't, then proposes a ready-made server.
    void createServer();
    /// Explains playit.gg and starts linking it in the private browser. False when the user said not now.
    bool offerPlayitSetup();
    /// playit.gg changed: once linking is done, go on to the server.
    void continuePlayitSetup();
    /// Shows what XylarJava will make (newest Vanilla, memory for this computer, playit.gg) and asks for consent.
    void proposeServer(bool usePlayit);
    /// Makes the server, starts it and makes its playit.gg tunnel.
    void makeServer(const LocalServerConfig& config);
    /// The whitelist: add and remove players by name.
    void managePlayers();
    /// The Minecraft name of the launcher's active account, empty when there's none.
    QString ownerName() const;
    void editServer();
    void deleteServer();
    void rebuildList();
    void watchServer(LocalServer* server);
    void showServer();
    void updateDetails();
    void updateAddresses();
    /// Puts the button's address on the clipboard and says so on the button for a moment.
    void copyAddress(QPushButton* button);
    void appendConsole(const QString& line);
    void copyModsFrom(const QString& folder);
    void updateStyle();
    QFrame* card(QWidget* parent);

    LocalServerList* m_servers;
    QNetworkAccessManager* m_network;
    QPointer<LocalServer> m_current;
    std::function<QList<QPair<QString, QString>>()> m_modSources;
    std::function<void(const QString&)> m_openFolder;

    QListWidget* m_list = nullptr;
    QStackedWidget* m_stack = nullptr;
    QLabel* m_name = nullptr;
    QLabel* m_subtitle = nullptr;
    QLabel* m_status = nullptr;
    QPushButton* m_createButton = nullptr;
    QPushButton* m_startButton = nullptr;
    QPushButton* m_folderButton = nullptr;
    QPushButton* m_playersButton = nullptr;
    QToolButton* m_addonsButton = nullptr;
    QPushButton* m_settingsButton = nullptr;
    QPushButton* m_deleteButton = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_playitName = nullptr;
    QLabel* m_playitValue = nullptr;
    QLabel* m_playitPort = nullptr;
    QPushButton* m_copyPlayit = nullptr;
    QPushButton* m_playitButton = nullptr;
    QLabel* m_publicName = nullptr;
    QLabel* m_publicValue = nullptr;
    QLabel* m_publicPort = nullptr;
    QPushButton* m_copyPublic = nullptr;
    QLabel* m_lanValue = nullptr;
    QLabel* m_lanPort = nullptr;
    QPushButton* m_copyLan = nullptr;
    QLabel* m_localValue = nullptr;
    QLabel* m_localPort = nullptr;
    QPushButton* m_copyLocal = nullptr;
    QLabel* m_networkMessage = nullptr;
    QPlainTextEdit* m_console = nullptr;
    QLineEdit* m_command = nullptr;
    QSet<QObject*> m_watched;
    QString m_appliedStyleSheet;
    bool m_updatingStyle = false;
    bool m_playitSetupPending = false;  // linking playit.gg from the setup: propose a server once it's done
    bool m_proposing = false;
};
