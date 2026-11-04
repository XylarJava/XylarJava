#include "LocalServersPage.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
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
#include <QRegularExpression>
#include <QScrollBar>
#include <QShowEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "DesktopServices.h"
#include "HardwareInfo.h"
#include "ServerSettingsDialog.h"
#include "minecraft/auth/AccountList.h"
#include "servers/PlayitTunnel.h"
#include "servers/LocalServerList.h"
#include "servers/ServerSoftware.h"
#include "settings/SettingsObject.h"
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

LocalServersPage::LocalServersPage(LocalServerList* servers, QNetworkAccessManager* network, QWidget* parent)
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
    connect(createButton, &QPushButton::clicked, this, &LocalServersPage::createServer);
    m_createButton = createButton;
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
    auto emptyText = new QLabel(tr("XylarJava makes it, picks the memory and starts it. It runs on this computer and friends "
                                   "join from anywhere through playit.gg, without seeing your IP. Up to %1 servers.")
                                    .arg(LocalServerList::MAX_SERVERS),
                                empty);
    emptyText->setObjectName(QStringLiteral("serverMuted"));
    emptyText->setWordWrap(true);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyText->setMaximumWidth(460);
    auto emptyButton = new QPushButton(tr("Create your first server"), empty);
    emptyButton->setObjectName(QStringLiteral("serverPrimary"));
    emptyButton->setCursor(Qt::PointingHandCursor);
    connect(emptyButton, &QPushButton::clicked, this, &LocalServersPage::createServer);
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
    m_playersButton = new QPushButton(QIcon::fromTheme(QStringLiteral("accounts")), QStringLiteral(" ") + tr("Players"), top);
    m_playersButton->setObjectName(QStringLiteral("serverSecondary"));
    m_playersButton->setToolTip(tr("Who can join this server"));
    connect(m_playersButton, &QPushButton::clicked, this, &LocalServersPage::managePlayers);
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
    connect(m_settingsButton, &QPushButton::clicked, this, &LocalServersPage::editServer);
    m_deleteButton = new QPushButton(QIcon::fromTheme(QStringLiteral("delete")), QStringLiteral(" ") + tr("Delete"), top);
    m_deleteButton->setObjectName(QStringLiteral("serverSecondary"));
    connect(m_deleteButton, &QPushButton::clicked, this, &LocalServersPage::deleteServer);
    actions->addWidget(m_startButton);
    actions->addWidget(m_folderButton);
    actions->addWidget(m_playersButton);
    actions->addWidget(m_addonsButton);
    actions->addWidget(m_settingsButton);
    actions->addStretch(1);
    actions->addWidget(m_deleteButton);
    topLayout->addLayout(actions);
    m_actionsCard = top;
    top->installEventFilter(this);
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
    // The addresses lead to this computer, so they stay hidden until asked: Copy still copies them.
    m_showAddresses = new QPushButton(join);
    m_showAddresses->setObjectName(QStringLiteral("serverSecondary"));
    m_showAddresses->setToolTip(tr("These addresses lead to your computer. Keep them hidden while you stream or share a screenshot: Copy works either way."));
    connect(m_showAddresses, &QPushButton::clicked, this, [this] { setAddressesShown(!m_addressesShown); });
    auto joinTitleRow = new QHBoxLayout();
    joinTitleRow->addWidget(joinTitle);
    joinTitleRow->addStretch(1);
    joinTitleRow->addWidget(m_showAddresses);
    joinLayout->addLayout(joinTitleRow);
    // One row per way in: the address in big letters, the port next to it, and Copy for exactly what to type.
    auto addressRow = [this, join, joinLayout](const QString& label, QLabel*& name, QLabel*& value, QLabel*& port, QPushButton*& copy) {
        auto row = new QHBoxLayout();
        row->setSpacing(10);
        name = new QLabel(label, join);
        name->setObjectName(QStringLiteral("serverMuted"));
        name->setFixedWidth(150);
        value = new QLabel(join);
        value->setObjectName(QStringLiteral("serverAddress"));
        value->setTextInteractionFlags(Qt::TextSelectableByMouse);
        port = new QLabel(join);
        port->setObjectName(QStringLiteral("serverPort"));
        port->setTextInteractionFlags(Qt::TextSelectableByMouse);
        copy = new QPushButton(tr("Copy"), join);
        copy->setObjectName(QStringLiteral("serverSecondary"));
        connect(copy, &QPushButton::clicked, this, [this, copy] { copyAddress(copy); });
        row->addWidget(name);
        row->addWidget(value);
        row->addWidget(port);
        row->addStretch(1);
        row->addWidget(copy);
        joinLayout->addLayout(row);
    };
    addressRow(tr("Protected (playit.gg)"), m_playitName, m_playitValue, m_playitPort, m_copyPlayit);
    addressRow(tr("Over the internet"), m_publicName, m_publicValue, m_publicPort, m_copyPublic);
    QLabel* unusedName = nullptr;
    addressRow(tr("Same Wi-Fi"), unusedName, m_lanValue, m_lanPort, m_copyLan);
    addressRow(tr("This computer"), unusedName, m_localValue, m_localPort, m_copyLocal);
    m_playitButton = new QPushButton(join);
    m_playitButton->setObjectName(QStringLiteral("serverSecondary"));
    connect(m_playitButton, &QPushButton::clicked, this, [this] {
        PlayitTunnel* playit = m_servers->playit();
        if (playit->state() == PlayitTunnel::State::Ready)
            playit->openPage(PlayitTunnel::tunnelsPage());
        else
            playit->link();
    });
    auto playitRow = new QHBoxLayout();
    playitRow->addWidget(m_playitButton);
    playitRow->addStretch(1);
    joinLayout->addLayout(playitRow);
    connect(m_servers->playit(), &PlayitTunnel::changed, this, &LocalServersPage::updateAddresses);
    connect(m_servers->playit(), &PlayitTunnel::changed, this, &LocalServersPage::continuePlayitSetup);
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

    connect(m_servers, &LocalServerList::listChanged, this, &LocalServersPage::rebuildList);
    rebuildList();
    updateStyle();
}

QFrame* LocalServersPage::card(QWidget* parent)
{
    auto frame = new QFrame(parent);
    frame->setObjectName(QStringLiteral("serverCard"));
    return frame;
}

void LocalServersPage::select(LocalServer* server)
{
    const int row = m_servers->servers().indexOf(server);
    if (row >= 0)
        m_list->setCurrentRow(row);
}

void LocalServersPage::rebuildList()
{
    const bool full = m_servers->isFull();
    m_createButton->setEnabled(!full);
    m_createButton->setToolTip(full ? tr("%1 servers is the most XylarJava hosts: delete one to make another.").arg(LocalServerList::MAX_SERVERS)
                                    : QString());
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

void LocalServersPage::watchServer(LocalServer* server)
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
    connect(server, &LocalServer::stateChanged, this, &LocalServersPage::updateDetails);
    connect(server, &LocalServer::addressesChanged, this, &LocalServersPage::updateAddresses);
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

void LocalServersPage::showServer()
{
    m_stack->setCurrentIndex(m_current ? 1 : 0);
    m_list->setVisible(!m_servers->servers().isEmpty());
    if (!m_current)
        return;
    m_console->setPlainText(hideAddresses(m_current->console().join('\n')));
    m_console->verticalScrollBar()->setValue(m_console->verticalScrollBar()->maximum());
    updateDetails();
    updateAddresses();
}

void LocalServersPage::updateDetails()
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
    m_addonsButton->setProperty("fullText", QStringLiteral(" ") + (config.software == ServerSoftware::Kind::Paper ? tr("Plugins") : tr("Mods")));
    m_addonsButton->setText(m_addonsButton->property("fullText").toString());
    fitActions();
    m_progress->setVisible(state == LocalServer::State::Installing);
    m_command->setEnabled(m_current->isRunning());
}

void LocalServersPage::copyAddress(QPushButton* button)
{
    const QString address = button->property("address").toString();
    if (address.isEmpty())
        return;
    QClipboard* clipboard = QApplication::clipboard();
    clipboard->setText(address);
    // On X11 the middle click pastes the selection, so fill that one too.
    if (clipboard->supportsSelection())
        clipboard->setText(address, QClipboard::Selection);
    button->setText(tr("Copied!"));
    QTimer::singleShot(1500, button, [button] { button->setText(tr("Copy")); });
}

bool LocalServersPage::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_actionsCard && event->type() == QEvent::Resize)
        fitActions();
    return QWidget::eventFilter(watched, event);
}

void LocalServersPage::fitActions()
{
    if (!m_actionsCard)
        return;
    const QList<QAbstractButton*> buttons = { m_folderButton, m_playersButton, m_addonsButton, m_settingsButton, m_deleteButton };
    // Width with every name shown: text, icon, padding and border of each button, plus the spacing.
    const QFontMetrics metrics(m_folderButton->font());
    const QMargins margins = m_actionsCard->layout()->contentsMargins();
    int needed = qMax(m_startButton->minimumWidth(), m_startButton->sizeHint().width()) + margins.left() + margins.right();
    for (QAbstractButton* button : buttons) {
        if (button->isHidden())
            continue;
        if (!button->property("fullText").isValid())
            button->setProperty("fullText", button->text());
        needed += metrics.horizontalAdvance(button->property("fullText").toString()) + button->iconSize().width() + 2 * 14 + 2 + 8;
    }
    const bool compact = needed > m_actionsCard->width();
    for (QAbstractButton* button : buttons) {
        const QString full = button->property("fullText").toString();
        button->setText(compact ? QString() : full);
        button->setToolTip(compact ? full.trimmed() : (button == m_playersButton ? tr("Who can join this server") : QString()));
    }
}

void LocalServersPage::updateAddresses()
{
    if (!m_current)
        return;
    const bool running = m_current->state() == LocalServer::State::Running;
    const QString port = tr("Port %1").arg(m_current->port());
    auto show = [this, &port](QLabel* value, QLabel* portLabel, QPushButton* copy, const QString& ip) {
        value->setText(ip.isEmpty() ? tr("-") : hideAddresses(ip));
        portLabel->setText(port);
        portLabel->setVisible(!ip.isEmpty());
        copy->setEnabled(!ip.isEmpty());
        copy->setProperty("address", ip.isEmpty() ? QString() : m_current->joinAddress(ip));
        copy->setToolTip(ip.isEmpty() ? QString() : hideAddresses(tr("Copies %1").arg(m_current->joinAddress(ip))));
    };
    show(m_publicValue, m_publicPort, m_copyPublic, m_current->publicIp());
    show(m_lanValue, m_lanPort, m_copyLan, m_current->lanIp());
    show(m_localValue, m_localPort, m_copyLocal, QStringLiteral("localhost"));

    // playit.gg: the protected address replaces the home IP, which isn't shown at all then.
    const bool usesPlayit = m_current->config().playit;
    PlayitTunnel* playit = m_servers->playit();
    const QString playitAddress = m_current->playitAddress();
    for (QWidget* w : { static_cast<QWidget*>(m_playitName), static_cast<QWidget*>(m_playitValue), static_cast<QWidget*>(m_copyPlayit) })
        w->setVisible(usesPlayit);
    for (QWidget* w : { static_cast<QWidget*>(m_publicName), static_cast<QWidget*>(m_publicValue), static_cast<QWidget*>(m_copyPublic) })
        w->setVisible(!usesPlayit);
    if (usesPlayit)
        m_publicPort->hide();
    // Minecraft Java tunnels have an SRV record: the name alone finds the right port.
    m_playitPort->setText(tr("Port: automatic"));
    m_playitPort->setVisible(usesPlayit && !playitAddress.isEmpty());
    m_playitValue->setText(playitAddress.isEmpty() ? tr("-") : hideAddresses(playitAddress));
    m_copyPlayit->setEnabled(!playitAddress.isEmpty());
    m_copyPlayit->setProperty("address", playitAddress);
    m_copyPlayit->setToolTip(playitAddress.isEmpty() ? tr("No playit.gg address yet") : hideAddresses(tr("Copies %1").arg(playitAddress)));

    QString message;
    m_playitButton->hide();
    if (usesPlayit) {
        switch (playit->state()) {
            case PlayitTunnel::State::Unsupported:
                message = tr("playit.gg isn't available on this system yet.");
                break;
            case PlayitTunnel::State::NotLinked:
                message = tr("Link your own playit.gg account (free) once: friends then get a playit address and never see your IP.");
                m_playitButton->setText(tr("Link playit.gg"));
                m_playitButton->show();
                break;
            case PlayitTunnel::State::Linking:
                message = playit->message();
                break;
            case PlayitTunnel::State::Ready:
                if (playitAddress.isEmpty()) {
                    playit->refresh();
                    message = tr("On playit.gg, add a Minecraft Java tunnel to 127.0.0.1, port %1. Its address shows up here.").arg(m_current->port());
                    m_playitButton->setText(tr("Open my playit.gg tunnels"));
                    m_playitButton->show();
                } else {
                    message = tr("Friends type %1 in Multiplayer > Add Server. playit.gg filters attacks and your IP stays hidden.").arg(playitAddress);
                    if (!running)
                        message += ' ' + tr("Start the server to open the tunnel.");
                }
                if (!playit->message().isEmpty())
                    message += QStringLiteral("\n") + playit->message();
                break;
        }
    } else {
        message = m_current->networkMessage();
        if (!running && message.isEmpty())
            message = tr("Start the server to get the internet address. Friends add it under Multiplayer > Add Server.");
        if (m_current->port() == 25565)
            message += (message.isEmpty() ? QString() : QStringLiteral("\n")) +
                       tr("Port 25565 is Minecraft's default, so the IP alone is enough: Copy gives exactly what to type.");
    }
    m_networkMessage->setText(hideAddresses(message));
    m_showAddresses->setText(m_addressesShown ? tr("Hide addresses") : tr("Show addresses"));
    m_publicValue->setProperty("ok", m_current->portOpen());
    m_publicValue->style()->unpolish(m_publicValue);
    m_publicValue->style()->polish(m_publicValue);
    m_playitValue->setProperty("ok", !playitAddress.isEmpty() && running);
    m_playitValue->style()->unpolish(m_playitValue);
    m_playitValue->style()->polish(m_playitValue);
}

void LocalServersPage::setAddressesShown(bool shown)
{
    m_addressesShown = shown;
    if (!m_current)
        return;
    const int scroll = m_console->verticalScrollBar()->value();
    m_console->setPlainText(hideAddresses(m_current->console().join('\n')));
    m_console->verticalScrollBar()->setValue(scroll);
    updateAddresses();
}

QString LocalServersPage::hideAddresses(const QString& text) const
{
    if (m_addressesShown || !m_current || text.isEmpty())
        return text;
    const QString hidden(12, QChar(0x2022));
    QString result = text;
    for (const QString& address : { m_current->playitAddress(), m_current->publicIp(), m_current->lanIp() })
        if (!address.isEmpty())
            result.replace(address, hidden, Qt::CaseInsensitive);
    // Any other IP too, such as the ones of the players joining in the console. This computer's own
    // (127.x, 0.0.0.0) stays: it's the same for everyone. IPv6 needs three colons, so times like 12:34:56 stay.
    static const QRegularExpression ipv4(QStringLiteral("(?<![\\d.])(\\d{1,3})\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}(?![\\d.])"));
    static const QRegularExpression ipv6(QStringLiteral("(?<![\\w.:])[0-9a-fA-F]{0,4}(?::[0-9a-fA-F]{0,5}){3,8}(?![\\w.:])"));
    QString masked;
    qsizetype last = 0;
    for (auto it = ipv4.globalMatch(result); it.hasNext();) {
        const QRegularExpressionMatch match = it.next();
        if (match.captured(1) == QLatin1String("127") || match.captured() == QLatin1String("0.0.0.0"))
            continue;
        masked += QStringView(result).mid(last, match.capturedStart() - last) + hidden;
        last = match.capturedEnd();
    }
    masked += QStringView(result).mid(last);
    return masked.replace(ipv6, hidden);
}

void LocalServersPage::appendConsole(const QString& line)
{
    QScrollBar* bar = m_console->verticalScrollBar();
    const bool atBottom = bar->value() >= bar->maximum() - 4;
    m_console->appendPlainText(hideAddresses(line));
    if (atBottom)
        bar->setValue(bar->maximum());
}

void LocalServersPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    // The first time ever the page opens: offer playit.gg (once the window is on screen).
    if (!APPLICATION->settings()->get("PlayitSetupOffered").toBool())
        QTimer::singleShot(0, this, [this] {
            if (APPLICATION->settings()->get("PlayitSetupOffered").toBool())
                return;
            APPLICATION->settings()->set("PlayitSetupOffered", true);
            if (m_servers->playit()->state() == PlayitTunnel::State::NotLinked)
                offerPlayitSetup();
        });
}

bool LocalServersPage::offerPlayitSetup()
{
    QMessageBox box(QMessageBox::Question, tr("Set up playit.gg"),
                    tr("<b>Let friends join from anywhere, safely</b><br><br>"
                       "With a free playit.gg account friends join your servers through playit.gg: they never see your "
                       "home IP, attacks are filtered and your router needs no setup.<br><br>"
                       "XylarJava opens playit.gg in a private Firefox window with the uBlock Origin ad blocker. "
                       "Create an account (or log in), accept playit.gg's terms and approve this computer.<br><br>"
                       "Then, if you agree, XylarJava makes your server and starts it for you."),
                    QMessageBox::NoButton, this);
    QPushButton* setUp = box.addButton(tr("Set up playit.gg"), QMessageBox::AcceptRole);
    box.addButton(tr("Not now"), QMessageBox::RejectRole);
    box.setDefaultButton(setUp);
    box.exec();
    if (box.clickedButton() != setUp)
        return false;
    m_playitSetupPending = true;
    m_servers->playit()->link();
    return true;
}

void LocalServersPage::continuePlayitSetup()
{
    if (!m_playitSetupPending)
        return;
    PlayitTunnel* playit = m_servers->playit();
    if (playit->state() == PlayitTunnel::State::NotLinked) {
        m_playitSetupPending = false;
        if (isVisible() && !playit->message().isEmpty())
            QMessageBox::warning(this, tr("playit.gg"), playit->message());
    } else if (playit->state() == PlayitTunnel::State::Ready) {
        m_playitSetupPending = false;
        // Out of this signal first: making the server opens a dialog.
        QTimer::singleShot(0, this, [this] {
            window()->raise();
            window()->activateWindow();
            proposeServer(true);
        });
    }
}

void LocalServersPage::createServer()
{
    if (m_servers->isFull()) {
        QMessageBox::information(this, tr("Create server"),
                                 tr("You have %1 servers, the most XylarJava hosts. Delete one to make another.").arg(LocalServerList::MAX_SERVERS));
        return;
    }
    PlayitTunnel* playit = m_servers->playit();
    switch (playit->state()) {
        case PlayitTunnel::State::NotLinked:
            // Linking ends with the same proposal; without playit.gg the server still works on the Wi-Fi and through the router.
            if (!offerPlayitSetup())
                proposeServer(false);
            return;
        case PlayitTunnel::State::Linking:
            m_playitSetupPending = true;
            QMessageBox::information(this, tr("playit.gg"),
                                     tr("Finish in the playit.gg window first (log in and approve this computer). "
                                        "Your server is made right after."));
            return;
        case PlayitTunnel::State::Ready:
            proposeServer(true);
            return;
        case PlayitTunnel::State::Unsupported:
            proposeServer(false);
            return;
    }
}

void LocalServersPage::proposeServer(bool usePlayit)
{
    if (m_servers->isFull() || m_proposing)
        return;
    m_proposing = true;
    setCursor(Qt::BusyCursor);
    ServerSoftware::fetchVersions(m_network, ServerSoftware::Kind::Vanilla, this, [this, usePlayit](const QStringList& versions, const QString& error) {
        unsetCursor();
        m_proposing = false;
        if (versions.isEmpty()) {
            QMessageBox::warning(this, tr("Create server"),
                                 tr("Couldn't get the list of Minecraft versions (%1). Check your connection and try again.")
                                     .arg(error.isEmpty() ? tr("empty list") : error));
            return;
        }
        LocalServerConfig config;
        config.name = m_servers->freeName(tr("My server"));
        config.software = ServerSoftware::Kind::Vanilla;
        config.minecraftVersion = versions.first();
        config.port = m_servers->freePort();
        config.playit = usePlayit;
        config.openPort = !usePlayit;
        config.onlineMode = false;
        config.whitelist = true;
        const QString owner = ownerName();
        // A quarter of this computer's memory, between 2 and 6 GB: plenty for a few friends, and the game still runs next to it.
        // Under 6 GB in all, 1 GB keeps the computer usable.
        const qint64 totalMb = qint64(HardwareInfo::totalRamMiB());
        const qint64 memoryMb = totalMb <= 0 ? 2048 : totalMb < 6144 ? 1024 : qBound(qint64(2048), totalMb / 4, qint64(6144));
        config.memoryMb = int(memoryMb / 1024 * 1024);

        QDialog dialog(this);
        dialog.setWindowTitle(tr("Create server"));
        dialog.setMinimumWidth(520);
        auto layout = new QVBoxLayout(&dialog);
        layout->setSpacing(10);
        auto heading = new QLabel(tr("XylarJava sets everything up for you"), &dialog);
        QFont headingFont = heading->font();
        headingFont.setPointSizeF(headingFont.pointSizeF() + 4);
        headingFont.setBold(true);
        heading->setFont(headingFont);
        layout->addWidget(heading);
        QStringList lines;
        lines << tr("<b>Name:</b> %1").arg(config.name.toHtmlEscaped())
              << tr("<b>Minecraft:</b> %1, Vanilla (the newest version)").arg(config.minecraftVersion)
              << tr("<b>Memory:</b> %1 GB (this computer has %2 GB)").arg(config.memoryMb / 1024).arg(qMax(qint64(1), (totalMb + 512) / 1024))
              << tr("<b>Java:</b> the one this version needs, downloaded if it's missing");
        if (usePlayit)
            lines << tr("<b>playit.gg:</b> a free tunnel on your account, so friends join without seeing your IP");
        else
            lines << tr("<b>Network:</b> friends on your Wi-Fi join right away; XylarJava opens your router for the rest when it can");
        lines << (owner.isEmpty() ? tr("<b>Players:</b> Microsoft and offline accounts, only the ones you add under Players")
                                  : tr("<b>Players:</b> Microsoft and offline accounts, only the ones you add under Players "
                                       "(%1 is already in)")
                                        .arg(owner.toHtmlEscaped()));
        lines << tr("<b>Then:</b> it downloads and starts by itself");
        auto summary = new QLabel(QStringLiteral("<p style='line-height:140%'>") + lines.join(QStringLiteral("<br>")) + QStringLiteral("</p>"), &dialog);
        summary->setWordWrap(true);
        layout->addWidget(summary);
        auto eula = new QCheckBox(&dialog);
        auto eulaText = new QLabel(tr("I accept the <a href=\"https://aka.ms/MinecraftEULA\">Minecraft EULA</a>"), &dialog);
        eulaText->setOpenExternalLinks(true);
        auto eulaRow = new QHBoxLayout();
        eulaRow->addWidget(eula);
        eulaRow->addWidget(eulaText, 1);
        layout->addLayout(eulaRow);
        auto buttons = new QDialogButtonBox(&dialog);
        QPushButton* create = buttons->addButton(tr("Create and start"), QDialogButtonBox::AcceptRole);
        QPushButton* customize = buttons->addButton(tr("Change settings..."), QDialogButtonBox::ActionRole);
        buttons->addButton(QDialogButtonBox::Cancel);
        create->setEnabled(false);
        create->setDefault(true);
        connect(eula, &QCheckBox::toggled, create, &QPushButton::setEnabled);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        bool customizing = false;
        connect(customize, &QPushButton::clicked, &dialog, [&dialog, &customizing] {
            customizing = true;
            dialog.reject();
        });
        layout->addWidget(buttons);
        const int result = dialog.exec();

        if (customizing) {
            config.eulaAccepted = eula->isChecked();
            ServerSettingsDialog settings(m_network, nullptr, this, &config);
            if (settings.exec() == QDialog::Accepted)
                makeServer(settings.config());
            return;
        }
        if (result != QDialog::Accepted || !eula->isChecked())
            return;
        config.eulaAccepted = true;
        makeServer(config);
    });
}

QString LocalServersPage::ownerName() const
{
    const auto account = APPLICATION->accounts()->defaultAccount();
    return account && LocalServer::validPlayerName(account->profileName()) ? account->profileName() : QString();
}

void LocalServersPage::managePlayers()
{
    if (!m_current)
        return;
    QPointer<LocalServer> server(m_current.data());
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Players - %1").arg(server->config().name));
    dialog.setMinimumWidth(440);
    auto layout = new QVBoxLayout(&dialog);
    auto lead = new QLabel(&dialog);
    lead->setWordWrap(true);
    layout->addWidget(lead);
    auto list = new QListWidget(&dialog);
    list->setMinimumHeight(180);
    layout->addWidget(list);
    auto addRow = new QHBoxLayout();
    auto nameEdit = new QLineEdit(&dialog);
    nameEdit->setPlaceholderText(tr("Minecraft name, e.g. Steve"));
    nameEdit->setMaxLength(16);
    auto add = new QPushButton(tr("Add"), &dialog);
    auto remove = new QPushButton(tr("Remove"), &dialog);
    addRow->addWidget(nameEdit, 1);
    addRow->addWidget(add);
    addRow->addWidget(remove);
    layout->addLayout(addRow);
    auto status = new QLabel(&dialog);
    status->setWordWrap(true);
    status->setEnabled(false);
    layout->addWidget(status);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    auto refresh = [server, list, lead, remove] {
        if (!server)
            return;
        list->clear();
        const QStringList names = server->allowedPlayers();
        list->addItems(names);
        if (!server->config().whitelist)
            lead->setText(tr("The whitelist is off: anyone with the address can join. Turn it on in the server's settings."));
        else if (!server->config().onlineMode)
            lead->setText(tr("Only these players can join, with a Microsoft or an offline account. The server can't check "
                             "who's who, so only add people you trust and don't share the list."));
        else
            lead->setText(tr("Only these players can join (Microsoft accounts only, the server is in online mode)."));
        remove->setEnabled(false);
    };
    connect(list, &QListWidget::currentRowChanged, remove, [remove](int row) { remove->setEnabled(row >= 0); });
    auto addPlayer = [server, nameEdit, add, status, refresh] {
        const QString name = nameEdit->text().trimmed();
        if (!server || name.isEmpty())
            return;
        add->setEnabled(false);
        status->setText(tr("Adding %1...").arg(name));
        server->allowPlayer(name, [nameEdit, add, status, refresh, name](const QString& error) {
            add->setEnabled(true);
            status->setText(error.isEmpty() ? tr("%1 can join now.").arg(LocalServer::knownSpelling(name)) : error);
            if (error.isEmpty())
                nameEdit->clear();
            refresh();
        });
    };
    connect(add, &QPushButton::clicked, &dialog, addPlayer);
    connect(nameEdit, &QLineEdit::returnPressed, &dialog, addPlayer);
    connect(remove, &QPushButton::clicked, &dialog, [server, list, status, refresh] {
        if (!server || !list->currentItem())
            return;
        const QString name = list->currentItem()->text();
        server->disallowPlayer(name);
        status->setText(tr("%1 can't join any more.").arg(name));
        // A running server rewrites whitelist.json itself a moment later.
        QTimer::singleShot(server->state() == LocalServer::State::Running ? 800 : 0, list, refresh);
    });
    connect(server, &LocalServer::whitelistChanged, list, refresh);
    refresh();
    dialog.exec();
}

void LocalServersPage::makeServer(const LocalServerConfig& config)
{
    LocalServer* server = m_servers->create(config);
    if (!server)
        return;
    select(server);
    // The one who makes it can join it.
    if (config.whitelist && !ownerName().isEmpty())
        server->allowPlayer(ownerName(), [](const QString&) {});
    server->start();  // downloads the server (and Java when missing), then starts it
    if (!config.playit)
        return;
    QPointer<LocalServer> guard(server);
    m_servers->playit()->createTunnel(config.name, config.port, [this, guard](const QString& error) {
        if (!error.isEmpty() && guard && isVisible())
            QMessageBox::warning(this, guard->config().name, error);
        updateAddresses();
    });
}

void LocalServersPage::editServer()
{
    if (!m_current || m_current->isRunning())
        return;
    const LocalServerConfig before = m_current->config();
    ServerSettingsDialog dialog(m_network, &before, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const LocalServerConfig after = dialog.config();
    m_current->setConfig(after);
    if (m_current->state() == LocalServer::State::NotInstalled)
        m_current->install();
    if (after.playit && (!before.playit || before.port != after.port) && m_servers->playit()->state() == PlayitTunnel::State::Ready) {
        QPointer<LocalServer> guard(m_current.data());
        m_servers->playit()->createTunnel(after.name, after.port, [this, guard](const QString& error) {
            if (!error.isEmpty() && guard && isVisible())
                QMessageBox::warning(this, guard->config().name, error);
            updateAddresses();
        });
    }
    updateDetails();
}

void LocalServersPage::deleteServer()
{
    if (!m_current || m_current->isRunning())
        return;
    const auto answer = QMessageBox::question(
        this, tr("Delete %1?").arg(m_current->config().name),
        m_current->config().playit
            ? tr("This deletes the server, its world and its playit.gg tunnel for good:\n%1").arg(QDir::toNativeSeparators(m_current->folder()))
            : tr("This deletes the server and its world for good:\n%1").arg(QDir::toNativeSeparators(m_current->folder())),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Yes)
        m_servers->remove(m_current);
}

void LocalServersPage::copyModsFrom(const QString& folder)
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

void LocalServersPage::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::StyleChange)
        updateStyle();
    QWidget::changeEvent(event);
}

void LocalServersPage::updateStyle()
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
    qss += QStringLiteral("#serverAddress { font-size: 20px; font-weight: 800; color: %1; }").arg(fg.name());
    qss += QStringLiteral("#serverAddress[ok=\"true\"] { color: %1; }").arg(green.name());
    qss += QStringLiteral("#serverPort { border-radius: 8px; padding: 3px 10px; font-weight: 700; background-color: %1; color: %2; }")
               .arg(rgba(accent, 40), accent.name());
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
