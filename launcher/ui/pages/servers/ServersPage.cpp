#include "ServersPage.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollBar>
#include <QStackedWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "ServerSettingsDialog.h"
#include "servers/LocalServerList.h"
#include "ui/widgets/HomePage.h"

namespace {
QString rgba(const QColor& c, int alpha)
{
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(alpha);
}

QColor mixColors(const QColor& a, const QColor& b, qreal t)
{
    const float f = float(qBound<qreal>(0.0, t, 1.0));
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f,
                            a.blueF() + (b.blueF() - a.blueF()) * f, 1.0f);
}

QString describe(const LocalServer* server)
{
    const QString dot = QStringLiteral("  ") + QChar(0x00B7) + QStringLiteral("  ");
    return ServerSoftware::displayName(server->config().software) + ' ' + server->config().minecraftVersion + dot + server->statusText();
}
}  // namespace

ServersPage::ServersPage(LocalServerList* servers, QNetworkAccessManager* network, QWidget* parent)
    : QWidget(parent), m_servers(servers), m_network(network)
{
    setObjectName(QStringLiteral("serversPage"));
    m_openFolder = [](const QString& path) { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); };

    auto root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // Header
    auto header = new QFrame(this);
    header->setObjectName(QStringLiteral("pageHeader"));
    auto headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(24, 18, 24, 14);
    auto title = new QLabel(tr("Your servers"), header);
    title->setObjectName(QStringLiteral("pageTitle"));
    auto hint = new QLabel(tr("Hosted on this computer, for you and your friends"), header);
    hint->setObjectName(QStringLiteral("pageHint"));
    auto createButton = new QPushButton(QIcon::fromTheme(QStringLiteral("new")), tr("Create server"), header);
    createButton->setObjectName(QStringLiteral("addInstanceButton"));
    createButton->setCursor(Qt::PointingHandCursor);
    connect(createButton, &QPushButton::clicked, this, &ServersPage::createServer);
    headerLayout->addWidget(title);
    headerLayout->addSpacing(12);
    headerLayout->addWidget(hint);
    headerLayout->addStretch(1);
    headerLayout->addWidget(createButton);
    root->addWidget(header);

    auto body = new QHBoxLayout();
    body->setContentsMargins(20, 16, 20, 20);
    body->setSpacing(16);
    root->addLayout(body, 1);

    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("serverList"));
    m_list->setFixedWidth(270);
    m_list->setIconSize(QSize(32, 32));
    m_list->setSpacing(3);
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        const auto all = m_servers->servers();
        m_current = row >= 0 && row < all.size() ? all.at(row) : nullptr;
        showServer();
    });
    body->addWidget(m_list);

    m_stack = new QStackedWidget(this);
    body->addWidget(m_stack, 1);

    // Empty state
    auto empty = new QWidget(m_stack);
    auto emptyLayout = new QVBoxLayout(empty);
    emptyLayout->addStretch(1);
    auto emptyIcon = new QLabel(empty);
    emptyIcon->setPixmap(QIcon::fromTheme(QStringLiteral("server")).pixmap(64, 64));
    emptyIcon->setAlignment(Qt::AlignCenter);
    auto emptyTitle = new QLabel(tr("Host a server for your friends"), empty);
    emptyTitle->setObjectName(QStringLiteral("serverEmptyTitle"));
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto emptyText = new QLabel(tr("Pick Vanilla, Fabric or Paper and any Minecraft version. It runs on this computer, "
                                   "friends on your Wi-Fi join right away and XylarJava opens your router for the rest."),
                                empty);
    emptyText->setObjectName(QStringLiteral("serverMuted"));
    emptyText->setWordWrap(true);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyText->setMaximumWidth(460);
    auto emptyButton = new QPushButton(tr("Create your first server"), empty);
    emptyButton->setObjectName(QStringLiteral("serverPrimary"));
    emptyButton->setCursor(Qt::PointingHandCursor);
    connect(emptyButton, &QPushButton::clicked, this, &ServersPage::createServer);
    emptyLayout->addWidget(emptyIcon);
    emptyLayout->addSpacing(8);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyText, 0, Qt::AlignHCenter);
    emptyLayout->addSpacing(12);
    emptyLayout->addWidget(emptyButton, 0, Qt::AlignHCenter);
    emptyLayout->addStretch(1);
    m_stack->addWidget(empty);

    // Details
    auto details = new QWidget(m_stack);
    auto detailsLayout = new QVBoxLayout(details);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(14);

    auto top = card(details);
    auto topLayout = new QVBoxLayout(top);
    topLayout->setContentsMargins(18, 16, 18, 16);
    topLayout->setSpacing(12);
    auto nameRow = new QHBoxLayout();
    auto nameColumn = new QVBoxLayout();
    nameColumn->setSpacing(2);
    m_name = new QLabel(top);
    m_name->setObjectName(QStringLiteral("serverName"));
    m_subtitle = new QLabel(top);
    m_subtitle->setObjectName(QStringLiteral("serverMuted"));
    nameColumn->addWidget(m_name);
    nameColumn->addWidget(m_subtitle);
    m_status = new QLabel(top);
    m_status->setObjectName(QStringLiteral("serverStatus"));
    nameRow->addLayout(nameColumn, 1);
    nameRow->addWidget(m_status, 0, Qt::AlignTop);
    topLayout->addLayout(nameRow);

    auto actions = new QHBoxLayout();
    actions->setSpacing(8);
    m_startButton = new QPushButton(top);
    m_startButton->setObjectName(QStringLiteral("serverPrimary"));
    m_startButton->setCursor(Qt::PointingHandCursor);
    m_startButton->setMinimumWidth(130);
    connect(m_startButton, &QPushButton::clicked, this, [this] {
        if (!m_current)
            return;
        if (m_current->isRunning())
            m_current->stop();
        else
            m_current->start();
    });
    m_folderButton = new QPushButton(QIcon::fromTheme(QStringLiteral("viewfolder")), QStringLiteral(" ") + tr("Folder"), top);
    m_folderButton->setObjectName(QStringLiteral("serverSecondary"));
    connect(m_folderButton, &QPushButton::clicked, this, [this] {
        if (m_current)
            m_openFolder(m_current->folder());
    });
    m_addonsButton = new QToolButton(top);
    m_addonsButton->setObjectName(QStringLiteral("serverSecondary"));
    m_addonsButton->setPopupMode(QToolButton::InstantPopup);
    m_addonsButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_addonsButton->setIcon(QIcon::fromTheme(QStringLiteral("loadermods")));
    m_addonsButton->setMenu(new QMenu(m_addonsButton));
    connect(m_addonsButton->menu(), &QMenu::aboutToShow, this, [this] {
        QMenu* menu = m_addonsButton->menu();
        menu->clear();
        if (!m_current)
            return;
        const QString addons = m_current->addonsPath();
        menu->addAction(tr("Open %1 folder").arg(QFileInfo(addons).fileName()), this, [this, addons] {
            QDir().mkpath(addons);
            m_openFolder(addons);
        });
        if (m_current->config().software == ServerSoftware::Kind::Fabric && m_modSources) {
            QMenu* copy = menu->addMenu(tr("Copy mods from an instance"));
            const auto sources = m_modSources();
            if (sources.isEmpty())
                copy->addAction(tr("No Fabric instances"))->setEnabled(false);
            for (const auto& source : sources) {
                const QString folder = source.second;
                copy->addAction(source.first, this, [this, folder] { copyModsFrom(folder); });
            }
        }
    });
    m_settingsButton = new QPushButton(QIcon::fromTheme(QStringLiteral("settings")), QStringLiteral(" ") + tr("Settings"), top);
    m_settingsButton->setObjectName(QStringLiteral("serverSecondary"));
    connect(m_settingsButton, &QPushButton::clicked, this, &ServersPage::editServer);
    m_deleteButton = new QPushButton(QIcon::fromTheme(QStringLiteral("delete")), QStringLiteral(" ") + tr("Delete"), top);
    m_deleteButton->setObjectName(QStringLiteral("serverSecondary"));
    connect(m_deleteButton, &QPushButton::clicked, this, &ServersPage::deleteServer);
    actions->addWidget(m_startButton);
    actions->addWidget(m_folderButton);
    actions->addWidget(m_addonsButton);
    actions->addWidget(m_settingsButton);
    actions->addStretch(1);
    actions->addWidget(m_deleteButton);
    topLayout->addLayout(actions);
    m_progress = new QProgressBar(top);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->hide();
    topLayout->addWidget(m_progress);
    detailsLayout->addWidget(top);

    // How friends join
    auto join = card(details);
    auto joinLayout = new QVBoxLayout(join);
    joinLayout->setContentsMargins(18, 14, 18, 14);
    joinLayout->setSpacing(8);
    auto joinTitle = new QLabel(tr("How friends join"), join);
    joinTitle->setObjectName(QStringLiteral("serverCardTitle"));
    joinLayout->addWidget(joinTitle);
    auto addressRow = [this, join, joinLayout](const QString& label, QLabel*& value, QPushButton*& copy) {
        auto row = new QHBoxLayout();
        auto name = new QLabel(label, join);
        name->setObjectName(QStringLiteral("serverMuted"));
        name->setFixedWidth(150);
        value = new QLabel(join);
        value->setObjectName(QStringLiteral("serverAddress"));
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        copy = new QPushButton(tr("Copy"), join);
        copy->setObjectName(QStringLiteral("serverSecondary"));
        QLabel* target = value;
        connect(copy, &QPushButton::clicked, this, [target] { QApplication::clipboard()->setText(target->text()); });
        row->addWidget(name);
        row->addWidget(value, 1);
        row->addWidget(copy);
        joinLayout->addLayout(row);
    };
    addressRow(tr("Same Wi-Fi"), m_lanValue, m_copyLan);
    addressRow(tr("Over the internet"), m_publicValue, m_copyPublic);
    m_networkMessage = new QLabel(join);
    m_networkMessage->setObjectName(QStringLiteral("serverMuted"));
    m_networkMessage->setWordWrap(true);
    joinLayout->addWidget(m_networkMessage);
    detailsLayout->addWidget(join);

    // Console
    auto consoleCard = card(details);
    auto consoleLayout = new QVBoxLayout(consoleCard);
    consoleLayout->setContentsMargins(14, 12, 14, 14);
    consoleLayout->setSpacing(8);
    auto consoleTitle = new QLabel(tr("Console"), consoleCard);
    consoleTitle->setObjectName(QStringLiteral("serverCardTitle"));
    consoleLayout->addWidget(consoleTitle);
    m_console = new QPlainTextEdit(consoleCard);
    m_console->setObjectName(QStringLiteral("serverConsole"));
    m_console->setReadOnly(true);
    m_console->setMaximumBlockCount(3000);
    m_console->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_console->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_console->setPlaceholderText(tr("The server's messages show up here while it runs."));
    consoleLayout->addWidget(m_console, 1);
    auto commandRow = new QHBoxLayout();
    m_command = new QLineEdit(consoleCard);
    m_command->setPlaceholderText(tr("Type a command, e.g. op YourName or whitelist add Friend"));
    auto send = new QPushButton(tr("Send"), consoleCard);
    send->setObjectName(QStringLiteral("serverSecondary"));
    auto sendCommand = [this] {
        if (m_current && m_current->isRunning()) {
            m_current->sendCommand(m_command->text());
            m_command->clear();
        }
    };
    connect(m_command, &QLineEdit::returnPressed, this, sendCommand);
    connect(send, &QPushButton::clicked, this, sendCommand);
    commandRow->addWidget(m_command, 1);
    commandRow->addWidget(send);
    consoleLayout->addLayout(commandRow);
    detailsLayout->addWidget(consoleCard, 1);
    m_stack->addWidget(details);

    connect(m_servers, &LocalServerList::listChanged, this, &ServersPage::rebuildList);
    rebuildList();
    updateStyle();
}

QFrame* ServersPage::card(QWidget* parent)
{
    auto frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("serverCard"));
    return frame;
}

void ServersPage::select(LocalServer* server)
{
    const int row = m_servers->servers().indexOf(server);
    if (row >= 0)
        m_list->setCurrentRow(row);
}

void ServersPage::rebuildList()
{
    LocalServer* keep = m_current;
    m_list->blockSignals(true);
    m_list->clear();
    for (LocalServer* server : m_servers->servers()) {
        auto item = new QListWidgetItem(QIcon::fromTheme(QStringLiteral("server")), server->config().name + '\n' + describe(server), m_list);
        item->setSizeHint(QSize(0, 52));
        watchServer(server);
    }
    m_list->blockSignals(false);
    const int row = m_servers->servers().indexOf(keep);
    if (row >= 0) {
        m_list->setCurrentRow(row);
        showServer();
    } else if (!m_servers->servers().isEmpty()) {
        m_list->setCurrentRow(0);
    } else {
        m_current = nullptr;
        showServer();
    }
}

void ServersPage::watchServer(LocalServer* server)
{
    // Connected once per server (lambdas can't use Qt::UniqueConnection).
    if (m_watched.contains(server))
        return;
    m_watched.insert(server);
    connect(server, &QObject::destroyed, this, [this, server] { m_watched.remove(server); });
    connect(server, &LocalServer::consoleLine, this, [this, server](const QString& line) {
        if (server == m_current)
            appendConsole(line);
    });
    connect(server, &LocalServer::stateChanged, this, &ServersPage::updateDetails);
    connect(server, &LocalServer::addressesChanged, this, &ServersPage::updateAddresses);
    connect(server, &LocalServer::installProgress, this, [this, server](qint64 done, qint64 total) {
        if (server != m_current)
            return;
        m_progress->setRange(0, total > 0 ? 1000 : 0);
        if (total > 0)
            m_progress->setValue(int(done * 1000 / total));
    });
    connect(server, &LocalServer::failed, this, [this, server](const QString& message) {
        if (server == m_current && isVisible())
            QMessageBox::warning(this, server->config().name, message);
    });
}

void ServersPage::showServer()
{
    m_stack->setCurrentIndex(m_current ? 1 : 0);
    m_list->setVisible(!m_servers->servers().isEmpty());
    if (!m_current)
        return;
    m_console->setPlainText(m_current->console().join('\n'));
    m_console->verticalScrollBar()->setValue(m_console->verticalScrollBar()->maximum());
    updateDetails();
    updateAddresses();
}

void ServersPage::updateDetails()
{
    // Refresh the list texts (status changes) without rebuilding it.
    const auto all = m_servers->servers();
    for (int i = 0; i < all.size() && i < m_list->count(); i++)
        m_list->item(i)->setText(all.at(i)->config().name + '\n' + describe(all.at(i)));
    if (!m_current)
        return;
    const auto& config = m_current->config();
    m_name->setText(config.name);
    const QString dot = QStringLiteral("  ") + QChar(0x00B7) + QStringLiteral("  ");
    m_subtitle->setText(ServerSoftware::displayName(config.software) + ' ' + config.minecraftVersion + dot +
                        tr("%1 GB").arg(config.memoryMb / 1024) + dot + tr("up to %1 players").arg(config.maxPlayers));
    m_status->setText(m_current->statusText());
    m_status->setProperty("state", m_current->state() == LocalServer::State::Running ? "online"
                                   : m_current->isRunning()                          ? "busy"
                                                                                     : "offline");
    m_status->style()->unpolish(m_status);
    m_status->style()->polish(m_status);

    const auto state = m_current->state();
    m_startButton->setEnabled(state != LocalServer::State::Installing && state != LocalServer::State::Stopping);
    m_startButton->setText(state == LocalServer::State::NotInstalled ? tr("Download")
                           : m_current->isRunning()                  ? tr("Stop")
                                                                     : tr("Start"));
    m_startButton->setProperty("stop", m_current->isRunning());
    m_startButton->style()->unpolish(m_startButton);
    m_startButton->style()->polish(m_startButton);
    m_settingsButton->setEnabled(!m_current->isRunning());
    m_deleteButton->setEnabled(!m_current->isRunning());
    const QString addons = ServerSoftware::addonsFolder(config.software);
    m_addonsButton->setVisible(!addons.isEmpty());
    m_addonsButton->setText(QStringLiteral(" ") + (config.software == ServerSoftware::Kind::Paper ? tr("Plugins") : tr("Mods")));
    m_progress->setVisible(state == LocalServer::State::Installing);
    m_command->setEnabled(m_current->isRunning());
}

void ServersPage::updateAddresses()
{
    if (!m_current)
        return;
    const bool running = m_current->state() == LocalServer::State::Running;
    m_lanValue->setText(m_current->lanAddress());
    m_publicValue->setText(m_current->publicAddress().isEmpty() ? tr("-") : m_current->publicAddress());
    m_copyPublic->setEnabled(!m_current->publicAddress().isEmpty());
    QString message = m_current->networkMessage();
    if (!running && message.isEmpty())
        message = tr("Start the server to get the addresses. Friends add it under Multiplayer > Add Server.");
    m_networkMessage->setText(message);
    m_publicValue->setProperty("ok", m_current->portOpen());
    m_publicValue->style()->unpolish(m_publicValue);
    m_publicValue->style()->polish(m_publicValue);
}

void ServersPage::appendConsole(const QString& line)
{
    QScrollBar* bar = m_console->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_console->appendPlainText(line);
    if (atBottom)
        bar->setValue(bar->maximum());
}

void ServersPage::createServer()
{
    ServerSettingsDialog dialog(m_network, nullptr, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    LocalServer* server = m_servers->create(dialog.config());
    select(server);
    server->install();
}

void ServersPage::editServer()
{
    if (!m_current || m_current->isRunning())
        return;
    const LocalServerConfig before = m_current->config();
    ServerSettingsDialog dialog(m_network, &before, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_current->setConfig(dialog.config());
    if (m_current->state() == LocalServer::State::NotInstalled)
        m_current->install();
    updateDetails();
}

void ServersPage::deleteServer()
{
    if (!m_current || m_current->isRunning())
        return;
    const auto answer = QMessageBox::question(
        this, tr("Delete %1?").arg(m_current->config().name),
        tr("This deletes the server and its world for good:\n%1").arg(QDir::toNativeSeparators(m_current->folder())),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
        m_servers->remove(m_current);
}

void ServersPage::copyModsFrom(const QString& folder)
{
    if (!m_current)
        return;
    const QString target = m_current->addonsPath();
    QDir().mkpath(target);
    int copied = 0;
    for (const QFileInfo& jar : QDir(folder).entryInfoList({ QStringLiteral("*.jar") }, QDir::Files)) {
        const QString destination = QDir(target).filePath(jar.fileName());
        if (QFileInfo::exists(destination))
            continue;
        if (QFile::copy(jar.absoluteFilePath(), destination))
            copied++;
    }
    QMessageBox::information(this, tr("Mods copied"),
                             tr("Copied %1 mods. Mods that only work on the client (minimaps, shaders, Sodium...) can stop the "
                                "server from starting: if it doesn't start, remove them from the mods folder.")
                                 .arg(copied));
}

void ServersPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::StyleChange)
        updateStyle();
    QWidget::changeEvent(event);
}

void ServersPage::updateStyle()
{
    if (m_updatingStyle)
        return;
    m_updatingStyle = true;
    const QPalette pal = QApplication::palette(this);
    const QColor bg = pal.color(QPalette::Window);
    const QColor fg = pal.color(QPalette::WindowText);
    const QColor accent = HomePage::accentFor(pal);
    const QColor accentText = HomePage::textOn(accent);
    const bool dark = bg.lightnessF() < 0.5f;
    const QColor card = dark ? bg.lighter(118) : bg.darker(104);
    const QColor muted = mixColors(fg, bg, 0.42);
    const QColor green(0x3F, 0xC4, 0x6B);
    const QColor danger(0xE5, 0x48, 0x4D);

    QString qss;
    qss += QStringLiteral("QFrame#serverCard { background-color: %1; border: 1px solid %2; border-radius: 14px; }").arg(card.name(), rgba(fg, 22));
    qss += QStringLiteral("QFrame#serverCard QLabel { background: transparent; border: none; }");
    qss += QStringLiteral("#serverName { font-size: 22px; font-weight: 800; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#serverEmptyTitle { font-size: 22px; font-weight: 800; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#serverCardTitle { font-size: 14px; font-weight: 700; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#serverMuted { color: %1; }").arg(muted.name());
    qss += QStringLiteral("#serverAddress { font-size: 15px; font-weight: 700; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#serverAddress[ok=\"true\"] { color: %1; }").arg(green.name());
    qss += QStringLiteral("#serverStatus { border-radius: 10px; padding: 4px 12px; font-weight: 700; background-color: %1; color: %2; }")
               .arg(rgba(fg, 18), muted.name());
    qss += QStringLiteral("#serverStatus[state=\"online\"] { background-color: %1; color: %2; }").arg(rgba(green, 48), green.name());
    qss += QStringLiteral("#serverStatus[state=\"busy\"] { background-color: %1; color: %2; }").arg(rgba(accent, 48), accent.name());
    qss += QStringLiteral(
               "#serverPrimary { background-color: %1; color: %2; border: none; border-radius: 10px; padding: 8px 20px; font-weight: 800; }"
               "#serverPrimary:hover { background-color: %3; }"
               "#serverPrimary:disabled { background-color: %4; color: %5; }"
               "#serverPrimary[stop=\"true\"] { background-color: %6; color: #FFFFFF; }")
               .arg(accent.name(), accentText.name(), accent.lighter(110).name(), rgba(fg, 18), muted.name(), danger.name());
    qss += QStringLiteral(
               "#serverSecondary { background-color: %1; color: %2; border: 1px solid %3; border-radius: 10px; padding: 7px 14px; font-weight: 600; }"
               "#serverSecondary:hover { background-color: %4; border-color: %5; }"
               "QToolButton#serverSecondary::menu-indicator { image: none; width: 0; }")
               .arg(rgba(fg, 10), fg.name(), rgba(fg, 26), rgba(fg, 20), rgba(accent, 160));
    qss += QStringLiteral(
               "QListWidget#serverList { background: transparent; border: none; outline: none; }"
               "QListWidget#serverList::item { background-color: %1; border: 1px solid %2; border-radius: 12px; padding: 8px; color: %3; }"
               "QListWidget#serverList::item:selected { border-color: %4; background-color: %5; color: %3; }")
               .arg(card.name(), rgba(fg, 22), fg.name(), accent.name(), rgba(accent, 30));
    qss += QStringLiteral("QPlainTextEdit#serverConsole { background-color: %1; border: 1px solid %2; border-radius: 10px; color: %3; padding: 6px; }")
               .arg(dark ? bg.darker(140).name() : QColor(0x1A, 0x1D, 0x26).name(), rgba(fg, 22), QStringLiteral("#D7DAE3"));
    qss += QStringLiteral("QProgressBar { background-color: %1; border: none; border-radius: 3px; }"
                          "QProgressBar::chunk { background-color: %2; border-radius: 3px; }")
               .arg(rgba(fg, 18), accent.name());
    if (qss != m_appliedStyleSheet) {
        m_appliedStyleSheet = qss;
        setStyleSheet(qss);
    }
    m_updatingStyle = false;
}
