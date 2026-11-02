#pragma once

#include <QDialog>

#include "servers/LocalServer.h"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QNetworkAccessManager;
class QSpinBox;

/// Create a server, or change one: software, Minecraft version, memory, port and game rules.
class ServerSettingsDialog : public QDialog {
    Q_OBJECT
   public:
    /// existing == nullptr: a new server.
    ServerSettingsDialog(QNetworkAccessManager* network, const LocalServerConfig* existing, QWidget* parent = nullptr);

    LocalServerConfig config() const;

   private:
    void loadVersions();
    void updateState();

    QNetworkAccessManager* m_network;
    LocalServerConfig m_original;
    bool m_editing = false;
    QLineEdit* m_name = nullptr;
    QComboBox* m_software = nullptr;
    QLabel* m_softwareHint = nullptr;
    QComboBox* m_version = nullptr;
    QLabel* m_versionStatus = nullptr;
    QSpinBox* m_memory = nullptr;
    QSpinBox* m_port = nullptr;
    QSpinBox* m_players = nullptr;
    QComboBox* m_gamemode = nullptr;
    QComboBox* m_difficulty = nullptr;
    QLineEdit* m_motd = nullptr;
    QCheckBox* m_offline = nullptr;
    QCheckBox* m_pvp = nullptr;
    QCheckBox* m_openPort = nullptr;
    QLineEdit* m_java = nullptr;
    QCheckBox* m_eula = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    int m_versionRequest = 0;
};
