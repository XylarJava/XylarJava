#include "WorldBackupsDialog.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "Application.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"
#include "ui/dialogs/ProgressDialog.h"

namespace {
// In MB of 1024 * 1024 bytes, as XylarJava Backups counts the space.
QString sizeText(qint64 bytes)
{
    if (bytes < 1024 * 1024)
        return WorldBackupsDialog::tr("%1 KB").arg(qMax<qint64>(1, (bytes + 1023) / 1024));
    const double mb = bytes / (1024.0 * 1024.0);
    return WorldBackupsDialog::tr("%1 MB").arg(QLocale().toString(mb, 'f', mb < 10 ? 1 : 0));
}
}  // namespace

WorldBackupsDialog::WorldBackupsDialog(MinecraftInstance* instance, const QString& worldDir, const QString& worldName, QWidget* parent)
    : QDialog(parent), m_instance(instance), m_worldDir(worldDir), m_worldName(worldName)
{
    setWindowTitle(tr("Cloud backups"));
    auto layout = new QVBoxLayout(this);

    m_usage = new QLabel(tr("Loading your backups..."), this);
    layout->addWidget(m_usage);
    m_usageBar = new QProgressBar(this);
    m_usageBar->setTextVisible(false);
    m_usageBar->setFixedHeight(8);
    m_usageBar->setRange(0, 1000);
    layout->addWidget(m_usageBar);

    m_message = new QLabel(this);
    m_message->setWordWrap(true);
    m_message->hide();
    layout->addWidget(m_message);

    m_list = new QTreeWidget(this);
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setHeaderLabels({ tr("World"), tr("Instance"), tr("Size"), tr("Backed up") });
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, &WorldBackupsDialog::updateButtons);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, &WorldBackupsDialog::restore);
    layout->addWidget(m_list, 1);

    m_auto = new QCheckBox(tr("Back up the worlds I play automatically when the game closes"), this);
    m_auto->setChecked(APPLICATION->settings()->get("XylarAutoWorldBackup").toBool());
    connect(m_auto, &QCheckBox::toggled, this, [](bool on) { APPLICATION->settings()->set("XylarAutoWorldBackup", on); });
    layout->addWidget(m_auto);

    auto buttons = new QHBoxLayout;
    m_backUp = new QPushButton(m_worldName.isEmpty() ? tr("Back up") : tr("Back up \"%1\"").arg(m_worldName), this);
    m_backUp->setToolTip(m_worldName.isEmpty() ? tr("Pick a world on the Worlds page first.") : tr("Upload a copy of this world now."));
    connect(m_backUp, &QPushButton::clicked, this, &WorldBackupsDialog::backUp);
    buttons->addWidget(m_backUp);
    m_restore = new QPushButton(tr("Restore"), this);
    m_restore->setToolTip(tr("Add the backup to this instance as a new world. Your worlds here stay as they are."));
    connect(m_restore, &QPushButton::clicked, this, &WorldBackupsDialog::restore);
    buttons->addWidget(m_restore);
    m_delete = new QPushButton(tr("Delete"), this);
    connect(m_delete, &QPushButton::clicked, this, &WorldBackupsDialog::removeSelected);
    buttons->addWidget(m_delete);
    buttons->addStretch(1);
    auto close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    resize(640, 420);
    updateButtons();
    refresh();
}

void WorldBackupsDialog::refresh()
{
    WorldBackups::list(this, [this](bool ok, const WorldBackups::Listing& listing, const QString& error) {
        m_loaded = ok;
        if (!ok) {
            m_usage->setText(tr("Your backups couldn't be loaded."));
            m_message->setText(error);
            m_message->show();
            updateButtons();
            return;
        }
        m_message->hide();
        m_listing = listing;
        m_usage->setText(tr("%1 of %2 used. Every world keeps its last %3 backups.")
                             .arg(sizeText(listing.used), sizeText(listing.quota))
                             .arg(listing.keep));
        m_usageBar->setValue(listing.quota > 0 ? int(qMin<qint64>(1000, listing.used * 1000 / listing.quota)) : 0);

        m_list->clear();
        for (int i = 0; i < listing.backups.size(); i++) {
            const auto& backup = listing.backups[i];
            auto item = new QTreeWidgetItem(m_list, { backup.world, backup.instance, sizeText(backup.size),
                                                      QLocale().toString(QDateTime::fromSecsSinceEpoch(backup.createdAt), QLocale::ShortFormat) });
            item->setData(0, Qt::UserRole, i);
        }
        if (listing.backups.isEmpty()) {
            m_message->setText(tr("No backups yet. Pick a world and press Back up."));
            m_message->show();
        }
        updateButtons();
    });
}

const WorldBackups::Backup* WorldBackupsDialog::selected() const
{
    const auto items = m_list->selectedItems();
    if (items.isEmpty())
        return nullptr;
    const int index = items.first()->data(0, Qt::UserRole).toInt();
    return index >= 0 && index < m_listing.backups.size() ? &m_listing.backups[index] : nullptr;
}

void WorldBackupsDialog::updateButtons()
{
    const bool running = m_instance && m_instance->isRunning();
    m_backUp->setEnabled(m_loaded && !m_busy && !m_worldDir.isEmpty() && !running);
    if (running)
        m_backUp->setToolTip(tr("Close the game first, so the world is saved."));
    m_restore->setEnabled(m_loaded && !m_busy && selected() && m_instance);
    m_delete->setEnabled(m_loaded && !m_busy && selected());
}

void WorldBackupsDialog::backUp()
{
    if (m_worldDir.isEmpty() || !m_instance)
        return;
    m_busy = true;
    updateButtons();
    auto task = std::make_unique<WorldBackupUpload>(m_worldDir, m_instance->name(), m_worldName);
    ProgressDialog progress(this);
    progress.execWithTask(task.get());
    if (task->getState() == Task::State::Failed)
        QMessageBox::warning(this, tr("Backup failed"), task->failReason());
    m_busy = false;
    refresh();
}

void WorldBackupsDialog::restore()
{
    const WorldBackups::Backup* backup = selected();
    if (!backup || !m_instance || m_busy)
        return;
    const WorldBackups::Backup copy = *backup;
    m_busy = true;
    updateButtons();
    auto task = std::make_unique<WorldBackupRestore>(copy, m_instance->worldDir());
    ProgressDialog progress(this);
    progress.execWithTask(task.get());
    m_busy = false;
    updateButtons();
    if (task->getState() == Task::State::Failed)
        QMessageBox::warning(this, tr("Restore failed"), task->failReason());
    else if (task->wasSuccessful())
        QMessageBox::information(this, tr("World restored"),
                                 tr("\"%1\" is now in this instance's worlds, with the date of the backup in its name.").arg(copy.world));
}

void WorldBackupsDialog::removeSelected()
{
    const WorldBackups::Backup* backup = selected();
    if (!backup || m_busy)
        return;
    const QString when = QLocale().toString(QDateTime::fromSecsSinceEpoch(backup->createdAt), QLocale::ShortFormat);
    if (QMessageBox::question(this, tr("Delete backup"), tr("Delete the backup of \"%1\" from %2? This can't be undone.").arg(backup->world, when),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
        return;
    m_busy = true;
    updateButtons();
    WorldBackups::remove(backup->id, this, [this](bool ok, const QString& error) {
        m_busy = false;
        if (!ok)
            QMessageBox::warning(this, tr("Delete backup"), error);
        refresh();
    });
}
