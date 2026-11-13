#include "XylarSkinsPage.h"
#include "ui/widgets/Skin3DPreview.h"
#include "ui_XylarSkinsPage.h"

#include <QDebug>
#include <QFileDialog>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMimeDatabase>
#include <QPainter>
#include <QPalette>
#include <QSaveFile>
#include <QSizePolicy>
#include <QUrlQuery>

#include "Application.h"
#include "minecraft/auth/AccountList.h"
#include "DesktopServices.h"
#include "settings/SettingsObject.h"
#include "minecraft/skins/SkinModel.h"
#include "minecraft/skins/XylarSkinClient.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"

// ─── Constructor / Destructor ──────────────────────────────────────────────────

XylarSkinsPage::XylarSkinsPage(QWidget* parent, MinecraftAccountPtr acct)
    : QDialog(parent), m_ui(new Ui::XylarSkinsPage), m_acct(acct)
{
    m_ui->setupUi(this);
    setWindowModality(Qt::WindowModal);
    setWindowTitle(tr("Skins & Capes"));

    setupSkinClient();
    setupPreviewWidget();
    setupCapeList();

    // Same skin, managed from the Xylar website: linking opens the site with a one-time code.
    m_websiteBtn = new QPushButton(tr("Manage on the Xylar website"), this);
    m_websiteBtn->setToolTip(tr("Opens the website signed in to your Xylar account, where you can change or remove this skin."));
    m_websiteBtn->setEnabled(false);
    if (auto* layout = m_ui->deleteBtn->parentWidget()->layout())
        layout->addWidget(m_websiteBtn);
    connect(m_websiteBtn, &QPushButton::clicked, this, &XylarSkinsPage::manageOnWebsite);

    m_accessButton = new QPushButton(tr("Skin access…"), this);
    m_accessButton->setToolTip(tr("Use the same skin profile on another device without changing who owns it."));
    auto* accessMenu = new QMenu(m_accessButton);
    accessMenu->addAction(tr("Import access from another device…"), this, &XylarSkinsPage::importSkinAccess);
    auto* exportAction = accessMenu->addAction(tr("Export access to another device…"), this, &XylarSkinsPage::exportSkinAccess);
    connect(accessMenu, &QMenu::aboutToShow, this,
            [this, exportAction] { exportAction->setEnabled(m_client && !m_client->playerKey(m_currentUuid).isEmpty()); });
    m_accessButton->setMenu(accessMenu);
    if (auto* layout = m_ui->deleteBtn->parentWidget()->layout())
        layout->addWidget(m_accessButton);

    // Connect model toggle buttons
    connect(m_ui->steveBtn, &QRadioButton::toggled, this, &XylarSkinsPage::on_steveBtn_toggled);
    connect(m_ui->alexBtn, &QRadioButton::toggled, this, &XylarSkinsPage::on_alexBtn_toggled);

    // Use the actual account texture when available; do not draw a fake Steve.
    restoreAccountSkinPreview();

    loadCurrentSkin();
}

XylarSkinsPage::~XylarSkinsPage()
{
    delete m_ui;
    delete m_previewSkin;
    if (m_ownsClient) {
        delete m_client;
    }
}

// ─── SkinProvider ─────────────────────────────────────────────────────────────

SkinModel* XylarSkinsPage::getSelectedSkin()
{
    return m_previewSkin;
}

QHash<QString, QImage> XylarSkinsPage::capes()
{
    return m_capeImages;
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void XylarSkinsPage::setupSkinClient()
{
    // Use global skin client if available and configured
    if (APPLICATION->skinClient() && !APPLICATION->skinClient()->backendUrl().isEmpty()) {
        m_client = APPLICATION->skinClient();
        m_ownsClient = false;
    } else {
        // Create our own (unconfigured) client as fallback - still allows cache usage
        APPLICATION->initSkinClient();
        m_client = APPLICATION->skinClient();
        m_ownsClient = false;
    }

    if (!m_client) {
        setStatusMessage(tr("Skin backend not configured. Set XylarSkinBackendUrl in settings."), true);
        return;
    }

    connect(m_client, &XylarSkinClient::playerRegistered, this, &XylarSkinsPage::onPlayerRegistered);
    connect(m_client, &XylarSkinClient::playerFetched, this, &XylarSkinsPage::onPlayerFetched);
    connect(m_client, &XylarSkinClient::skinMetadataReceived, this, &XylarSkinsPage::onSkinMetadataReceived);
    connect(m_client, &XylarSkinClient::skinDownloaded, this, &XylarSkinsPage::onSkinDownloaded);
    connect(m_client, &XylarSkinClient::skinUploaded, this, &XylarSkinsPage::onSkinUploaded);
    connect(m_client, &XylarSkinClient::skinDeleted, this, &XylarSkinsPage::onSkinDeleted);
    connect(m_client, &XylarSkinClient::requestFailed, this, &XylarSkinsPage::onRequestFailed);
    connect(m_client, &XylarSkinClient::siteLinkReady, this, &XylarSkinsPage::onSiteLinkReady);
    connect(m_client, &XylarSkinClient::capesListed, this, &XylarSkinsPage::onCapesListed);
    connect(m_client, &XylarSkinClient::capeMetadataReceived, this, &XylarSkinsPage::onCapeMetadataReceived);
    connect(m_client, &XylarSkinClient::capeDownloaded, this, &XylarSkinsPage::onCapeDownloaded);
    connect(m_client, &XylarSkinClient::capeChanged, this, &XylarSkinsPage::onCapeChanged);
}

void XylarSkinsPage::setupPreviewWidget()
{
    if (SkinOpenGLWindow::hasOpenGL()) {
        m_skinPreview = new SkinOpenGLWindow(this, palette().color(QPalette::Normal, QPalette::Base));
        QWidget* container = QWidget::createWindowContainer(m_skinPreview, this);
        container->setMinimumSize(180, 280);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_ui->skinPreviewLayout->addWidget(container);
        m_ui->skinPreviewLayout->addWidget(m_skinPreview->createViewControls(this));
    } else {
        m_softwarePreview = new Skin3DPreview(this);
        m_softwarePreview->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_softwarePreview->setMinimumSize(120, 200);
        m_ui->skinPreviewLayout->addWidget(m_softwarePreview);
    }
}

// ─── UUID Helpers ─────────────────────────────────────────────────────────────

QString XylarSkinsPage::xylarUuid() const
{
    if (!m_currentUuid.isEmpty()) {
        return m_currentUuid;
    }

    if (!m_acct) {
        return QString();
    }

    const auto stored=m_acct->accountData()->yggdrasilToken.extra.value("xylarProfileId").toString();
    return stored.isEmpty() ? XylarSkinClient::computeOfflineUUID(m_acct->profileName()) : stored;
}

QString XylarSkinsPage::currentModel() const
{
    return m_ui->alexBtn->isChecked() ? "slim" : "classic";
}

// ─── Load Current Skin ────────────────────────────────────────────────────────

void XylarSkinsPage::loadCurrentSkin()
{
    if (!m_acct) {
        setStatusMessage(tr("No account selected."), true);
        return;
    }

    QString uuid = xylarUuid();
    if (uuid.isEmpty()) {
        setStatusMessage(tr("Cannot determine player UUID."), true);
        return;
    }
    m_currentUuid = uuid;

    // Try cache first
    if (m_client && m_client->hasCachedSkin(uuid)) {
        refreshFromCache();
    }

    // If no backend configured, stop here
    if (!m_client || m_client->backendUrl().isEmpty()) {
        if (!m_client || !m_client->hasCachedSkin(uuid)) {
            m_ui->currentSkinLabel->setText(tr("Backend not configured — skin cannot be loaded from server."));
        }
        return;
    }

    // Register player then fetch metadata
    setStatusMessage(tr("Loading skin information…"));
    setUIEnabled(false);

    QString username = m_acct->profileName();
    if (!username.isEmpty()) {
        QString msaUuid;
        if (m_acct->accountType() == AccountType::MSA) {
            msaUuid = m_acct->profileId();
            if (!msaUuid.contains('-') && msaUuid.length() == 32) {
                msaUuid.insert(8, '-');
                msaUuid.insert(13, '-');
                msaUuid.insert(18, '-');
                msaUuid.insert(23, '-');
            }
        }
        m_client->registerPlayer(username, msaUuid, m_acct->accessToken(),QString(),xylarUuid());
    } else {
        m_client->fetchPlayer(uuid);
    }
}

void XylarSkinsPage::refreshFromCache()
{
    if (!m_client || m_currentUuid.isEmpty()) {
        return;
    }

    QByteArray cached = m_client->loadCachedSkin(m_currentUuid);
    if (!cached.isEmpty()) {
        applySkinToPreview(cached, currentModel());
        m_ui->currentSkinLabel->setText(tr("Skin loaded from local cache."));
        m_ui->deleteBtn->setEnabled(true);
    }
}

void XylarSkinsPage::applySkinToPreview(const QByteArray& pngData, const QString& model)
{
    QImage texture;
    if (!texture.loadFromData(pngData, "PNG")) {
        return;
    }

    SkinModel::Model skinModel = (model == "slim") ? SkinModel::SLIM : SkinModel::CLASSIC;
    delete m_previewSkin;
    m_previewSkin = new SkinModel(texture, skinModel);

    m_previewSkin->setCapeId(m_currentCapeHash);

    if (m_skinPreview) {
        m_skinPreview->updateScene(m_previewSkin);
    } else if (m_softwarePreview) {
        refreshSoftwarePreview();
    }

    // Sync the model radio buttons
    if (model == "slim") {
        m_ui->alexBtn->setChecked(true);
    } else {
        m_ui->steveBtn->setChecked(true);
    }
}

void XylarSkinsPage::restoreAccountSkinPreview()
{
    QByteArray skinData;
    QString model = "classic";
    if (m_acct && m_acct->accountData()) {
        const auto& skin = m_acct->accountData()->minecraftProfile.skin;
        skinData = skin.data;
        if (skin.variant.compare("slim", Qt::CaseInsensitive) == 0) {
            model = "slim";
        }
    }

    QImage texture;
    if (!skinData.isEmpty()) {
        texture.loadFromData(skinData, "PNG");
    }
    if (texture.isNull()) {
        texture = QImage(64, 64, QImage::Format_RGBA8888);
        texture.fill(Qt::transparent);
    }

    const auto skinModel = model == "slim" ? SkinModel::SLIM : SkinModel::CLASSIC;
    delete m_previewSkin;
    m_previewSkin = new SkinModel(texture, skinModel);
    m_previewSkin->setCapeId(m_currentCapeHash);
    if (m_skinPreview) {
        m_skinPreview->updateScene(m_previewSkin);
    } else if (m_softwarePreview) {
        refreshSoftwarePreview();
    }
    m_ui->alexBtn->setChecked(model == "slim");
    m_ui->steveBtn->setChecked(model != "slim");
}

void XylarSkinsPage::setStatusMessage(const QString& msg, bool isError)
{
    m_ui->statusLabel->setText(msg);
    QPalette p = m_ui->statusLabel->palette();
    p.setColor(QPalette::WindowText, isError ? QColor(Qt::red) : QApplication::palette().color(QPalette::WindowText));
    m_ui->statusLabel->setPalette(p);
}

void XylarSkinsPage::setUIEnabled(bool enabled)
{
    m_uiEnabled = enabled;
    m_ui->capeList->setEnabled(enabled && m_ui->capeList->count() > 0);
    m_ui->uploadCapeBtn->setEnabled(enabled && !m_currentUuid.isEmpty() && m_client && !m_client->backendUrl().isEmpty());
    if (m_websiteBtn)
        m_websiteBtn->setEnabled(enabled && !m_currentUuid.isEmpty() && m_client && !m_client->backendUrl().isEmpty());
    if (m_accessButton)
        m_accessButton->setEnabled(enabled && !m_currentUuid.isEmpty() && m_client && !m_client->backendUrl().isEmpty());
    m_ui->uploadBtn->setEnabled(enabled && !m_selectedFilePath.isEmpty());
    m_ui->selectFileBtn->setEnabled(enabled);
    m_ui->steveBtn->setEnabled(enabled);
    m_ui->alexBtn->setEnabled(enabled);
}

// ─── Button Handlers ──────────────────────────────────────────────────────────

void XylarSkinsPage::on_selectFileBtn_clicked()
{
    auto filter = QMimeDatabase().mimeTypeForName("image/png").filterString();
    QString path = QFileDialog::getOpenFileName(this, tr("Select Skin PNG (64×64)"), QDir::homePath(), filter);
    if (path.isEmpty()) {
        return;
    }

    // Validate immediately
    QString errMsg;
    if (!XylarSkinClient::validateSkinPng(path, &errMsg)) {
        setStatusMessage(tr("Invalid skin: %1").arg(errMsg), true);
        return;
    }

    m_selectedFilePath = path;
    m_ui->filePathEdit->setText(path);
    m_ui->uploadBtn->setEnabled(true);
    setStatusMessage(tr("Skin file ready. Choose Classic or Slim, then click Upload."));

    // Show immediate preview
    QFile f(path);
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray bytes = f.readAll();
        f.close();
        applySkinToPreview(bytes, currentModel());
    }
}

void XylarSkinsPage::on_uploadBtn_clicked()
{
    if (m_selectedFilePath.isEmpty()) {
        setStatusMessage(tr("Please select a skin file first."), true);
        return;
    }

    QString uuid = m_currentUuid.isEmpty() ? xylarUuid() : m_currentUuid;
    if (uuid.isEmpty()) {
        setStatusMessage(tr("Cannot determine player UUID."), true);
        return;
    }

    if (!m_client || m_client->backendUrl().isEmpty()) {
        setStatusMessage(tr("Backend not configured. Cannot upload skin."), true);
        return;
    }

    setStatusMessage(tr("Uploading skin…"));
    setUIEnabled(false);
    m_client->uploadSkin(uuid, m_selectedFilePath, currentModel());
}

void XylarSkinsPage::on_deleteBtn_clicked()
{
    QString uuid = m_currentUuid.isEmpty() ? xylarUuid() : m_currentUuid;
    if (uuid.isEmpty()) {
        setStatusMessage(tr("Cannot determine player UUID."), true);
        return;
    }

    if (!m_client) {
        // Even without backend, clear the cache locally
        XylarSkinClient localClient;
        localClient.deleteCachedSkin(uuid);
        m_currentSkinHash = QString();
        m_ui->deleteBtn->setEnabled(false);
        m_ui->currentSkinLabel->setText(tr("Skin removed from local cache."));

        restoreAccountSkinPreview();
        return;
    }

    if (m_client->backendUrl().isEmpty()) {
        // No backend — just clear cache
        m_client->deleteCachedSkin(uuid);
        m_currentSkinHash = QString();
        m_ui->deleteBtn->setEnabled(false);
        m_ui->currentSkinLabel->setText(tr("Skin removed from local cache."));

        restoreAccountSkinPreview();
        return;
    }

    setStatusMessage(tr("Deleting skin from server…"));
    setUIEnabled(false);
    m_client->deleteSkin(uuid);
}

void XylarSkinsPage::on_steveBtn_toggled(bool checked)
{
    if (!checked) return;
    if (m_previewSkin) {
        m_previewSkin->setModel(SkinModel::CLASSIC);
        if (m_skinPreview) {
            m_skinPreview->updateScene(m_previewSkin);
        } else if (m_softwarePreview) {
            refreshSoftwarePreview();
        }
    }
}

void XylarSkinsPage::on_alexBtn_toggled(bool checked)
{
    if (!checked) return;
    if (m_previewSkin) {
        m_previewSkin->setModel(SkinModel::SLIM);
        if (m_skinPreview) {
            m_skinPreview->updateScene(m_previewSkin);
        } else if (m_softwarePreview) {
            refreshSoftwarePreview();
        }
    }
}

// ─── Signal Handlers from XylarSkinClient ─────────────────────────────────────

void XylarSkinsPage::onPlayerRegistered(const QString& uuid, const QString& username, const QString& skinHash, const QString& skinModel)
{
    if (!m_acct || username.compare(m_acct->profileName(), Qt::CaseInsensitive) != 0)
        return;
    m_currentUuid = uuid;
    m_acct->accountData()->yggdrasilToken.extra["xylarProfileId"] = uuid;
    APPLICATION->accounts()->saveList();
    m_currentSkinHash = skinHash;
    requestCapes();

    if (!skinHash.isEmpty()) {
        // Player has a skin: check cache or download
        if (m_client && m_client->hasCachedSkin(uuid)) {
            QByteArray cached = m_client->loadCachedSkin(uuid);
            // Verify hash matches
            if (XylarSkinClient::computeSha256(cached) == skinHash) {
                applySkinToPreview(cached, skinModel);
                m_ui->currentSkinLabel->setText(tr("This is the skin XylarJava players see."));
                m_ui->deleteBtn->setEnabled(true);
                setStatusMessage(tr("Ready."));
                setUIEnabled(true);
                return;
            }
        }
        // Need to (re)download
        m_client->downloadSkin(uuid, skinHash, skinModel);
    } else {
        QByteArray accountSkin;
        QString accountModel = "classic";
        if (m_acct && m_acct->accountData()) {
            const auto& skin = m_acct->accountData()->minecraftProfile.skin;
            accountSkin = skin.data;
            if (skin.variant.compare("slim", Qt::CaseInsensitive) == 0) {
                accountModel = "slim";
            }
        }
        QString validationError;
        if (XylarSkinClient::validateSkinPngData(accountSkin, &validationError)) {
            setStatusMessage(tr("Adding this account's current Minecraft skin to XylarSkins…"));
            setUIEnabled(false);
            m_client->uploadSkinData(uuid, accountSkin, accountModel);
            return;
        }
        restoreAccountSkinPreview();
        m_ui->currentSkinLabel->setText(tr("No Xylar skin saved yet. Choose a 64×64 PNG to share it with XylarJava players."));
        m_ui->deleteBtn->setEnabled(false);
        setStatusMessage(tr("Ready — no skin uploaded yet."));
        setUIEnabled(true);
    }
}

void XylarSkinsPage::onPlayerFetched(const QString& uuid, const QString& username, const QString& skinHash, const QString& skinModel)
{
    onPlayerRegistered(uuid, username, skinHash, skinModel);
}

void XylarSkinsPage::onSkinMetadataReceived(const QString& uuid, bool hasSkin, const QString& skinHash, const QString& model)
{
    (void)uuid;
    if (hasSkin && !skinHash.isEmpty()) {
        m_currentSkinHash = skinHash;

        // Verify cache
        if (m_client && m_client->hasCachedSkin(m_currentUuid)) {
            QByteArray cached = m_client->loadCachedSkin(m_currentUuid);
            if (XylarSkinClient::computeSha256(cached) == skinHash) {
                applySkinToPreview(cached, model);
                m_ui->currentSkinLabel->setText(tr("This is the skin XylarJava players see."));
                m_ui->deleteBtn->setEnabled(true);
                setStatusMessage(tr("Ready."));
                setUIEnabled(true);
                return;
            }
        }
        m_client->downloadSkin(m_currentUuid, skinHash, model);
    } else {
        restoreAccountSkinPreview();
        m_ui->currentSkinLabel->setText(tr("No Xylar skin saved yet. Showing this account's Minecraft skin, if available."));
        m_ui->deleteBtn->setEnabled(false);
        setStatusMessage(tr("Ready."));
        setUIEnabled(true);
    }
}

void XylarSkinsPage::onSkinDownloaded(const QString& uuid, const QString& localPath, const QString& model, const QString& skinHash)
{
    (void)uuid;
    m_currentSkinHash = skinHash;

    QFile f(localPath);
    if (f.open(QIODevice::ReadOnly)) {
        QByteArray bytes = f.readAll();
        f.close();
        applySkinToPreview(bytes, model);
    }

    m_ui->currentSkinLabel->setText(tr("This is the skin XylarJava players see."));
    m_ui->deleteBtn->setEnabled(true);
    setStatusMessage(tr("Skin loaded successfully."));
    setUIEnabled(true);
}

void XylarSkinsPage::onSkinUploaded(const QString& uuid, const QString& skinHash, const QString& model)
{
    (void)uuid;
    m_currentSkinHash = skinHash;

    // Reload preview from newly uploaded file
    if (!m_selectedFilePath.isEmpty()) {
        QFile f(m_selectedFilePath);
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray bytes = f.readAll();
            f.close();
            applySkinToPreview(bytes, model);
        }
    } else if (m_acct && m_acct->accountData()) {
        applySkinToPreview(m_acct->accountData()->minecraftProfile.skin.data, model);
    }

    m_selectedFilePath.clear();
    m_ui->filePathEdit->clear();
    m_ui->uploadBtn->setEnabled(false);
    m_ui->currentSkinLabel->setText(tr("Skin uploaded. Restart Minecraft to apply it."));
    m_ui->deleteBtn->setEnabled(true);
    setStatusMessage(tr("✓ Skin uploaded. Restart Minecraft to apply it."));
    setUIEnabled(true);
}

void XylarSkinsPage::onSkinDeleted(const QString& uuid)
{
    (void)uuid;
    m_currentSkinHash.clear();

    restoreAccountSkinPreview();
    m_ui->currentSkinLabel->setText(tr("Skin removed."));
    m_ui->deleteBtn->setEnabled(false);
    setStatusMessage(tr("✓ Skin deleted."));
    setUIEnabled(true);
}

void XylarSkinsPage::onRequestFailed(const QString& action, const QString& errorMessage, int httpStatusCode)
{
    if ((action == "registerPlayer" || action == "fetchPlayer") &&
        (httpStatusCode == 401 || httpStatusCode == 403 || httpStatusCode == 409)) {
        restoreAccountSkinPreview();
        setStatusMessage(errorMessage, true);
        setUIEnabled(false);
        if (m_accessButton)
            m_accessButton->setEnabled(true);
        return;
    }
    QString msg;
    if (action == "registerPlayer" || action == "fetchPlayer") {
        // If server unreachable, fall back to local cache
        if (m_client && m_client->hasCachedSkin(m_currentUuid.isEmpty() ? xylarUuid() : m_currentUuid)) {
            refreshFromCache();
            msg = tr("Server unreachable — showing cached skin. (%1)").arg(errorMessage);
            setStatusMessage(msg, false);
        } else {
            msg = tr("Failed to connect to skin server: %1").arg(errorMessage);
            setStatusMessage(msg, true);
            m_ui->currentSkinLabel->setText(tr("Could not load skin information."));
        }
    } else if (action == "uploadSkin") {
        msg = tr("Upload failed: %1").arg(errorMessage);
        setStatusMessage(msg, true);
    } else if (action == "deleteSkin") {
        msg = tr("Delete failed: %1").arg(errorMessage);
        setStatusMessage(msg, true);
    } else if (action == "downloadSkin") {
        msg = tr("Download failed: %1").arg(errorMessage);
        setStatusMessage(msg, true);
        // Still allow fallback from cache
        if (m_client && m_client->hasCachedSkin(m_currentUuid)) {
            refreshFromCache();
        }
    } else if (action == "downloadCape") {
        // Only a thumbnail or the preview is missing; the cape itself is fine.
        qWarning() << "XylarSkins: cape download failed:" << errorMessage;
        return;
    } else if (action == "fetchCapes" || action == "fetchCapeMetadata") {
        msg = tr("Couldn't load capes: %1").arg(errorMessage);
        setStatusMessage(msg, true);
    } else if (action == "setCape" || action == "uploadCape" || action == "removeCape") {
        msg = tr("Couldn't change the cape: %1").arg(errorMessage);
        setStatusMessage(msg, true);
        selectCurrentCape();
    } else if (action == "siteLink") {
        msg = tr("Couldn't open the website: %1").arg(errorMessage);
        setStatusMessage(msg, true);
    } else {
        msg = tr("Error (%1): %2").arg(action, errorMessage);
        setStatusMessage(msg, true);
    }

    setUIEnabled(true);
}

// ─── Xylar website ────────────────────────────────────────────────────────────

void XylarSkinsPage::manageOnWebsite()
{
    if (!m_client || m_currentUuid.isEmpty())
        return;
    setStatusMessage(tr("Opening the Xylar website…"));
    setUIEnabled(false);
    m_client->startSiteLink(m_currentUuid);
}

void XylarSkinsPage::importSkinAccess()
{
    if (!m_client || m_currentUuid.isEmpty())
        return;
    const QString path = QFileDialog::getOpenFileName(this, tr("Import Skin Access"), QDir::homePath(),
                                                      tr("Skin access files (*.xylar-skin-access);;All files (*)"));
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatusMessage(tr("Could not read skin access: %1").arg(file.errorString()), true);
        return;
    }
    QString error;
    if (!m_client->importPlayerAccess(m_currentUuid, file.read(16385), &error)) {
        setStatusMessage(error, true);
        return;
    }
    loadCurrentSkin();
}

void XylarSkinsPage::exportSkinAccess()
{
    if (!m_client || m_currentUuid.isEmpty())
        return;
    const QByteArray access = m_client->exportPlayerAccess(m_currentUuid);
    if (access.isEmpty()) {
        setStatusMessage(tr("This device does not own the skin profile. Export access from the device where changing the skin works."),
                         true);
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Save Skin Access — Keep This File Private"),
                                                QDir::home().filePath(m_currentUuid + QStringLiteral(".xylar-skin-access")),
                                                tr("Skin access files (*.xylar-skin-access)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QLatin1String(".xylar-skin-access")))
        path += QStringLiteral(".xylar-skin-access");
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        setStatusMessage(tr("Could not save skin access: %1").arg(file.errorString()), true);
        return;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(access) != access.size() || !file.commit()) {
        setStatusMessage(tr("Could not save skin access: %1").arg(file.errorString()), true);
        return;
    }
    setStatusMessage(tr("Access saved. Import this file on your other device to change this skin there. Keep the file private."));
}

void XylarSkinsPage::onSiteLinkReady(const QString& uuid, const QString& code)
{
    if (uuid != m_currentUuid)
        return;
    QString site = APPLICATION->settings()->get("XylarSiteUrl").toString().trimmed();
    while (site.endsWith('/'))
        site.chop(1);
    QUrl url(site + QStringLiteral("/skins/link"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("code"), code);
    url.setQuery(query);
    DesktopServices::openUrl(url);
    setStatusMessage(tr("Finish on the website: sign in and confirm. The code works for 10 minutes."));
    setUIEnabled(true);
}

// ─── Capes ────────────────────────────────────────────────────────────────────

namespace {
constexpr int CapeIdRole = Qt::UserRole;
constexpr int CapeHashRole = Qt::UserRole + 1;
const QString NoCape = QStringLiteral("none");
const QString CustomCape = QStringLiteral("custom");
}  // namespace

void XylarSkinsPage::setupCapeList()
{
    auto* list = m_ui->capeList;
    list->setViewMode(QListView::IconMode);
    list->setIconSize(QSize(40, 64));
    list->setGridSize(QSize(92, 100));
    list->setMinimumHeight(2 * 100 + 8);  // both rows of capes, without scrolling
    list->setMovement(QListView::Static);
    list->setResizeMode(QListView::Adjust);
    list->setWrapping(true);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    // Clicks only: selecting the worn cape from code must not send it again.
    connect(list, &QListWidget::itemClicked, this, &XylarSkinsPage::onCapeItemClicked);
}

void XylarSkinsPage::requestCapes()
{
    if (m_capesRequested || !m_client || m_client->backendUrl().isEmpty() || m_currentUuid.isEmpty())
        return;
    m_capesRequested = true;
    m_client->fetchCapes();
    m_client->fetchCapeMetadata(m_currentUuid);
}

QIcon XylarSkinsPage::capeIcon(const QImage& cape) const
{
    QPixmap canvas(40, 64);
    canvas.fill(Qt::transparent);
    QPainter painter(&canvas);
    if (!cape.isNull()) {
        // The outside of the cape: 10x16 pixels at (1, 1) of a 64x32 texture, scaled up for HD capes.
        const int scale = qMax(1, cape.width() / 64);
        painter.drawImage(QRect(0, 0, 40, 64), cape.copy(scale, scale, 10 * scale, 16 * scale));
    } else {
        painter.setPen(QPen(palette().color(QPalette::Mid), 1, Qt::DashLine));
        painter.drawRect(0, 0, 39, 63);
    }
    return QIcon(canvas);
}

void XylarSkinsPage::addCapeItem(const QString& id, const QString& name, const QString& hash)
{
    auto* list = m_ui->capeList;
    for (int i = 0; i < list->count(); ++i) {
        auto* item = list->item(i);
        if (item->data(CapeIdRole).toString() == id) {
            item->setData(CapeHashRole, hash);
            item->setIcon(capeIcon(m_capeImages.value(hash)));
            return;
        }
    }
    auto* item = new QListWidgetItem(capeIcon(m_capeImages.value(hash)), name);
    item->setData(CapeIdRole, id);
    item->setData(CapeHashRole, hash);
    item->setToolTip(name);
    list->addItem(item);
}

void XylarSkinsPage::removeCapeItem(const QString& id)
{
    auto* list = m_ui->capeList;
    for (int i = list->count() - 1; i >= 0; --i) {
        if (list->item(i)->data(CapeIdRole).toString() == id)
            delete list->takeItem(i);
    }
}

void XylarSkinsPage::selectCurrentCape()
{
    auto* list = m_ui->capeList;
    for (int i = 0; i < list->count(); ++i) {
        auto* item = list->item(i);
        const bool worn = m_currentCapeHash.isEmpty() ? item->data(CapeIdRole).toString() == NoCape
                                                      : item->data(CapeHashRole).toString() == m_currentCapeHash;
        if (worn) {
            list->setCurrentItem(item);
            return;
        }
    }
    list->clearSelection();
}

void XylarSkinsPage::showCapeInPreview()
{
    if (m_previewSkin)
        m_previewSkin->setCapeId(m_currentCapeHash);
    if (m_skinPreview)
        m_skinPreview->updateCape(m_capeImages.value(m_currentCapeHash));  // a null image hides it
    else
        refreshSoftwarePreview();
}

void XylarSkinsPage::refreshSoftwarePreview()
{
    if (!m_softwarePreview || !m_previewSkin)
        return;
    m_softwarePreview->setSkin(m_previewSkin->getTexture(), m_previewSkin->getModel() == SkinModel::SLIM);
    m_softwarePreview->setCape(m_capeImages.value(m_currentCapeHash));
}

void XylarSkinsPage::onCapesListed(const QJsonArray& capes)
{
    m_ui->capeList->clear();
    addCapeItem(NoCape, tr("None"), QString());
    for (const auto& value : capes) {
        const QJsonObject cape = value.toObject();
        const QString hash = cape.value("hash").toString();
        addCapeItem(cape.value("id").toString(), cape.value("name").toString(), hash);
        m_client->downloadCape(hash);
    }
    if (m_currentCapeId == CustomCape)
        addCapeItem(CustomCape, tr("Your cape"), m_currentCapeHash);
    selectCurrentCape();
    m_ui->capeList->setEnabled(m_uiEnabled);
}

void XylarSkinsPage::onCapeMetadataReceived(const QString& uuid, bool hasCape, const QString& capeHash, const QString& capeId)
{
    if (uuid != m_currentUuid)
        return;
    m_currentCapeHash = hasCape ? capeHash : QString();
    m_currentCapeId = hasCape ? capeId : QString();
    if (m_currentCapeId == CustomCape && m_ui->capeList->count() > 0)
        addCapeItem(CustomCape, tr("Your cape"), m_currentCapeHash);
    if (!m_currentCapeHash.isEmpty())
        m_client->downloadCape(m_currentCapeHash);
    selectCurrentCape();
    showCapeInPreview();
}

void XylarSkinsPage::onCapeDownloaded(const QString& hash, const QByteArray& pngData)
{
    QImage image;
    if (!image.loadFromData(pngData, "PNG"))
        return;
    m_capeImages.insert(hash, image);
    auto* list = m_ui->capeList;
    for (int i = 0; i < list->count(); ++i) {
        if (list->item(i)->data(CapeHashRole).toString() == hash)
            list->item(i)->setIcon(capeIcon(image));
    }
    if (hash == m_currentCapeHash)
        showCapeInPreview();
}

void XylarSkinsPage::onCapeItemClicked(QListWidgetItem* item)
{
    if (!item || !m_uiEnabled || !m_client || m_currentUuid.isEmpty())
        return;
    const QString id = item->data(CapeIdRole).toString();
    const QString hash = item->data(CapeHashRole).toString();
    if (id == NoCape) {
        if (m_currentCapeHash.isEmpty())
            return;
        setStatusMessage(tr("Taking the cape off…"));
        setUIEnabled(false);
        m_client->removeCape(m_currentUuid);
        return;
    }
    // "Your cape" is only listed while it's worn.
    if (hash == m_currentCapeHash || id == CustomCape)
        return;
    setStatusMessage(tr("Putting on %1…").arg(item->text()));
    setUIEnabled(false);
    m_client->setCape(m_currentUuid, id);
}

void XylarSkinsPage::on_uploadCapeBtn_clicked()
{
    if (!m_client || m_currentUuid.isEmpty())
        return;
    auto filter = QMimeDatabase().mimeTypeForName("image/png").filterString();
    const QString path = QFileDialog::getOpenFileName(this, tr("Select Cape PNG (64×32)"), QDir::homePath(), filter);
    if (path.isEmpty())
        return;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setStatusMessage(tr("Cannot read the cape file: %1").arg(file.errorString()), true);
        return;
    }
    const QByteArray bytes = file.readAll();
    QString errMsg;
    if (!XylarSkinClient::validateCapePngData(bytes, &errMsg)) {
        setStatusMessage(tr("Invalid cape: %1").arg(errMsg), true);
        return;
    }
    // Known before the answer comes back, so the preview shows it straight away.
    QImage image;
    if (image.loadFromData(bytes, "PNG"))
        m_capeImages.insert(XylarSkinClient::computeSha256(bytes), image);
    setStatusMessage(tr("Uploading cape…"));
    setUIEnabled(false);
    m_client->uploadCapeData(m_currentUuid, bytes);
}

void XylarSkinsPage::onCapeChanged(const QString& uuid, const QString& capeHash, const QString& capeId)
{
    if (uuid != m_currentUuid)
        return;
    m_currentCapeHash = capeHash;
    m_currentCapeId = capeId;
    if (capeId == CustomCape)
        addCapeItem(CustomCape, tr("Your cape"), capeHash);
    else
        removeCapeItem(CustomCape);  // an uploaded cape is gone once another is worn
    if (!capeHash.isEmpty() && !m_capeImages.contains(capeHash))
        m_client->downloadCape(capeHash);
    selectCurrentCape();
    showCapeInPreview();
    setStatusMessage(capeHash.isEmpty() ? tr("✓ Cape taken off. Restart Minecraft to apply it.")
                                        : tr("✓ Cape changed. Restart Minecraft to apply it."));
    setUIEnabled(true);
}
