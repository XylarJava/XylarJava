#include "ServerSettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "servers/PlayitTunnel.h"

ServerSettingsDialog::ServerSettingsDialog(QNetworkAccessManager* network,
                                           const LocalServerConfig* existing,
                                           QWidget* parent,
                                           const LocalServerConfig* defaults)
    : QDialog(parent), m_network(network), m_editing(existing != nullptr)
{
    if (existing)
        m_original = *existing;
    else if (defaults)
        m_original = *defaults;
    setWindowTitle(m_editing ? tr("Server settings") : tr("Create a server"));
    setMinimumWidth(640);

    auto layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto heading = new QLabel(m_editing ? tr("Server settings") : tr("Host a server for your friends"), this);
    heading->setObjectName(QStringLiteral("dialogHeading"));
    QFont headingFont = heading->font();
    headingFont.setPointSizeF(headingFont.pointSizeF() + 5);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);
    auto lead = new QLabel(tr("It runs on this computer while XylarJava is open. Friends on your Wi-Fi join right away; "
                              "over the internet XylarJava opens the port on your router for you when it can."),
                           this);
    lead->setWordWrap(true);
    lead->setEnabled(false);
    layout->addWidget(lead);

    // Game
    auto game = new QGroupBox(tr("Game"), this);
    auto gameForm = new QFormLayout(game);
    m_name = new QLineEdit(m_editing || defaults ? m_original.name : tr("My server"), game);
    m_name->setMaxLength(48);
    gameForm->addRow(tr("Name"), m_name);

    m_software = new QComboBox(game);
    m_software->addItem(tr("Vanilla - the game as Mojang ships it"), ServerSoftware::id(ServerSoftware::Kind::Vanilla));
    m_software->addItem(tr("Fabric - for Fabric mods"), ServerSoftware::id(ServerSoftware::Kind::Fabric));
    m_software->addItem(tr("Paper - fast, with plugins"), ServerSoftware::id(ServerSoftware::Kind::Paper));
    m_software->setCurrentIndex(qMax(0, m_software->findData(ServerSoftware::id(m_original.software))));
    gameForm->addRow(tr("Software"), m_software);
    m_softwareHint = new QLabel(game);
    m_softwareHint->setEnabled(false);
    gameForm->addRow(QString(), m_softwareHint);

    m_version = new QComboBox(game);
    m_version->setEditable(false);
    m_versionStatus = new QLabel(game);
    m_versionStatus->setEnabled(false);
    auto versionRow = new QHBoxLayout();
    versionRow->addWidget(m_version, 1);
    versionRow->addWidget(m_versionStatus);
    gameForm->addRow(tr("Minecraft version"), versionRow);

    m_gamemode = new QComboBox(game);
    m_gamemode->addItem(tr("Survival"), QStringLiteral("survival"));
    m_gamemode->addItem(tr("Creative"), QStringLiteral("creative"));
    m_gamemode->addItem(tr("Adventure"), QStringLiteral("adventure"));
    m_gamemode->setCurrentIndex(qMax(0, m_gamemode->findData(m_original.gamemode)));
    gameForm->addRow(tr("Game mode"), m_gamemode);

    m_difficulty = new QComboBox(game);
    m_difficulty->addItem(tr("Peaceful"), QStringLiteral("peaceful"));
    m_difficulty->addItem(tr("Easy"), QStringLiteral("easy"));
    m_difficulty->addItem(tr("Normal"), QStringLiteral("normal"));
    m_difficulty->addItem(tr("Hard"), QStringLiteral("hard"));
    m_difficulty->setCurrentIndex(qMax(0, m_difficulty->findData(m_original.difficulty)));
    gameForm->addRow(tr("Difficulty"), m_difficulty);

    m_players = new QSpinBox(game);
    m_players->setRange(1, 100);
    m_players->setValue(m_original.maxPlayers);
    gameForm->addRow(tr("Max players"), m_players);

    m_motd = new QLineEdit(m_original.motd, game);
    m_motd->setPlaceholderText(tr("Shown in your friends' server list"));
    gameForm->addRow(tr("Description"), m_motd);

    m_pvp = new QCheckBox(tr("Players can hurt each other (PvP)"), game);
    m_pvp->setChecked(m_original.pvp);
    gameForm->addRow(QString(), m_pvp);
    m_offline = new QCheckBox(tr("Allow players without a Microsoft account (offline mode)"), game);
    m_offline->setChecked(!m_original.onlineMode);
    gameForm->addRow(QString(), m_offline);
    layout->addWidget(game);

    // Computer and network
    auto host = new QGroupBox(tr("This computer"), this);
    auto hostForm = new QFormLayout(host);
    m_memory = new QSpinBox(host);
    m_memory->setRange(1, 64);
    m_memory->setSuffix(tr(" GB"));
    m_memory->setValue(qMax(1, m_original.memoryMb / 1024));
    hostForm->addRow(tr("Memory"), m_memory);
    m_port = new QSpinBox(host);
    m_port->setRange(1024, 65535);
    m_port->setValue(m_original.port);
    hostForm->addRow(tr("Port"), m_port);
    m_openPort = new QCheckBox(tr("Open the port on my router automatically (UPnP)"), host);
    m_openPort->setChecked(m_original.openPort);
    hostForm->addRow(QString(), m_openPort);
    m_playit = new QCheckBox(tr("Protect with playit.gg (free): friends get a playit address instead of your IP, attacks are filtered"), host);
    m_playit->setChecked(m_original.playit);
    m_playit->setEnabled(PlayitTunnel::supported() || m_original.playit);
    if (!PlayitTunnel::supported())
        m_playit->setToolTip(tr("Not available on this system yet."));
    hostForm->addRow(QString(), m_playit);
    // With playit nothing needs opening on the router, and opening it would publish the home IP again.
    auto syncOpenPort = [this] { m_openPort->setEnabled(!m_playit->isChecked()); };
    connect(m_playit, &QCheckBox::toggled, this, syncOpenPort);
    syncOpenPort();

    m_java = new QLineEdit(m_original.javaPath, host);
    m_java->setPlaceholderText(tr("Automatic"));
    auto browse = new QPushButton(tr("Browse..."), host);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, tr("Choose Java"));
        if (!path.isEmpty())
            m_java->setText(path);
    });
    auto javaRow = new QHBoxLayout();
    javaRow->addWidget(m_java, 1);
    javaRow->addWidget(browse);
    hostForm->addRow(tr("Java"), javaRow);
    layout->addWidget(host);

    m_eula = new QCheckBox(this);
    m_eula->setText(tr("I accept the Minecraft End User License Agreement"));
    m_eula->setChecked(m_original.eulaAccepted);
    auto eulaLink = new QLabel(QStringLiteral("<a href=\"https://aka.ms/MinecraftEULA\">%1</a>").arg(tr("Read the EULA")), this);
    eulaLink->setOpenExternalLinks(true);
    auto eulaRow = new QHBoxLayout();
    eulaRow->addWidget(m_eula);
    eulaRow->addStretch(1);
    eulaRow->addWidget(eulaLink);
    layout->addLayout(eulaRow);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_buttons->button(QDialogButtonBox::Ok)->setText(m_editing ? tr("Save") : tr("Create server"));
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(m_buttons);

    connect(m_software, &QComboBox::currentIndexChanged, this, [this] { loadVersions(); });
    connect(m_version, &QComboBox::currentIndexChanged, this, [this] { updateState(); });
    connect(m_eula, &QCheckBox::toggled, this, [this] { updateState(); });
    connect(m_name, &QLineEdit::textChanged, this, [this] { updateState(); });
    loadVersions();
}

void ServerSettingsDialog::loadVersions()
{
    const auto kind = ServerSoftware::fromId(m_software->currentData().toString());
    switch (kind) {
        case ServerSoftware::Kind::Vanilla:
            m_softwareHint->setText(tr("No mods. Anyone with the same version can join."));
            break;
        case ServerSoftware::Kind::Fabric:
            m_softwareHint->setText(tr("For Fabric mods. Your friends need the same mods."));
            break;
        case ServerSoftware::Kind::Paper:
            m_softwareHint->setText(tr("Faster than Vanilla, with plugins. Friends join with plain Minecraft."));
            break;
    }
    const QString wanted = m_version->currentText().isEmpty() ? m_original.minecraftVersion : m_version->currentText();
    m_version->clear();
    m_version->setEnabled(false);
    m_versionStatus->setText(tr("Loading..."));
    m_versionStatus->show();
    updateState();
    const int request = ++m_versionRequest;
    ServerSoftware::fetchVersions(m_network, kind, this, [this, request, wanted](const QStringList& versions, const QString& error) {
        if (request != m_versionRequest)
            return;  // the software changed meanwhile
        if (!error.isEmpty() || versions.isEmpty()) {
            m_versionStatus->setText(tr("Couldn't load versions"));
            m_versionStatus->setToolTip(error);
            if (!wanted.isEmpty())
                m_version->addItem(wanted);
            m_version->setEnabled(!wanted.isEmpty());
            updateState();
            return;
        }
        m_version->addItems(versions);
        const int index = m_version->findText(wanted);
        m_version->setCurrentIndex(index >= 0 ? index : 0);
        m_version->setEnabled(true);
        m_versionStatus->clear();
        m_versionStatus->setToolTip(QString());
        m_versionStatus->hide();
        updateState();
    });
}

void ServerSettingsDialog::updateState()
{
    const bool ok = m_eula->isChecked() && !m_name->text().trimmed().isEmpty() && !m_version->currentText().isEmpty();
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

LocalServerConfig ServerSettingsDialog::config() const
{
    LocalServerConfig c = m_original;
    c.name = m_name->text().trimmed();
    c.software = ServerSoftware::fromId(m_software->currentData().toString());
    c.minecraftVersion = m_version->currentText();
    c.memoryMb = m_memory->value() * 1024;
    c.port = m_port->value();
    c.maxPlayers = m_players->value();
    c.gamemode = m_gamemode->currentData().toString();
    c.difficulty = m_difficulty->currentData().toString();
    c.motd = m_motd->text().trimmed();
    c.onlineMode = !m_offline->isChecked();
    c.pvp = m_pvp->isChecked();
    c.playit = m_playit->isChecked();
    c.openPort = m_openPort->isChecked() && !c.playit;
    c.javaPath = m_java->text().trimmed();
    c.eulaAccepted = m_eula->isChecked();
    return c;
}
