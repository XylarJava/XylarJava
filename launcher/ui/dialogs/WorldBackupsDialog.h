#pragma once

#include <QDialog>

#include "social/WorldBackups.h"

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class MinecraftInstance;

/**
 * The player's world backups in the cloud: how much of the space is used, every backup (from any
 * instance), and buttons to back up the selected world, restore a backup into this instance as a
 * new world, or delete one.
 */
class WorldBackupsDialog : public QDialog {
    Q_OBJECT

   public:
    /// @p worldDir and @p worldName: the world selected on the Worlds page, empty for none.
    WorldBackupsDialog(MinecraftInstance* instance, const QString& worldDir, const QString& worldName, QWidget* parent = nullptr);

   private:
    void refresh();
    void updateButtons();
    void backUp();
    void restore();
    void removeSelected();
    const WorldBackups::Backup* selected() const;

    MinecraftInstance* m_instance = nullptr;
    QString m_worldDir;
    QString m_worldName;
    WorldBackups::Listing m_listing;
    bool m_loaded = false;
    bool m_busy = false;

    QLabel* m_usage = nullptr;
    QProgressBar* m_usageBar = nullptr;
    QLabel* m_message = nullptr;
    QTreeWidget* m_list = nullptr;
    QCheckBox* m_auto = nullptr;
    QPushButton* m_backUp = nullptr;
    QPushButton* m_restore = nullptr;
    QPushButton* m_delete = nullptr;
};
