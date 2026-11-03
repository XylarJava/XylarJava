#include "XylarSkinsPage.h"
#include "ui_XylarSkinsPage.h"

#include <QFileDialog>
#include <QLabel>
#include <QMimeDatabase>
#include <QPalette>
#include <QSizePolicy>
#include <QUrlQuery>

#include "Application.h"
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
    setWindowTitle(tr("Skins"));

    setupSkinClient();
    setupPreviewWidget();

    // Same skin, managed from the Xylar website: linking opens the site with a one-time code.
    m_websiteBtn = new QPushButton(tr("Manage on the Xylar website"), this);
    m_websiteBtn->setToolTip(tr("Opens the website signed in to your Xylar account, where you can change or remove this skin."));
    m_websiteBtn->setEnabled(false);
    if (auto* layout = m_ui->deleteBtn->parentWidget()->layout())
        layout->addWidget(m_websiteBtn);
    connect(m_websiteBtn, &QPushButton::clicked, this, &XylarSkinsPage::manageOnWebsite);

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
    return {};
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
}

void XylarSkinsPage::setupPreviewWidget()
{
    if (SkinOpenGLWindow::hasOpenGL()) {
        m_skinPreview = new SkinOpenGLWindow(this, palette().color(QPalette::Normal, QPalette::Base));
        QWidget* container = QWidget::createWindowContainer(m_skinPreview, this);
        container->setMinimumSize(180, 280);
        container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_ui->skinPreviewLayout->addWidget(container);
    } else {
        m_skinPreviewLabel = new QLabel(this);
        m_skinPreviewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        m_skinPreviewLabel->setAlignment(Qt::AlignCenter);
        m_skinPreviewLabel->setMinimumSize(120, 200);
        m_ui->skinPreviewLayout->addWidget(m_skinPreviewLabel);
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

    // For premium (MSA) accounts: use minecraftProfile.id (real UUID)
    QString profileId = m_acct->profileId();
    if (!profileId.isEmpty()) {
        // Insert dashes if not present (id128 format)
        if (!profileId.contains('-') && profileId.length() == 32) {
            profileId.insert(8, '-');
            profileId.insert(13, '-');
            profileId.insert(18, '-');
            profileId.insert(23, '-');
        }
        return profileId;
    }

    // For offline accounts: compute offline UUID from username
    QString username = m_acct->profileName();
    if (!username.isEmpty()) {
        return XylarSkinClient::computeOfflineUUID(username);
    }

    return QString();
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
        m_client->registerPlayer(username, msaUuid);
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

    if (m_skinPreview) {
        m_skinPreview->updateScene(m_previewSkin);
    } else if (m_skinPreviewLabel) {
        m_skinPreviewLabel->setPixmap(
            QPixmap::fromImage(m_previewSkin->getPreview())
                .scaled(m_skinPreviewLabel->size().isValid() ? m_skinPreviewLabel->size() : QSize(200, 200),
                        Qt::KeepAspectRatio, Qt::FastTransformation));
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
    if (m_skinPreview) {
        m_skinPreview->updateScene(m_previewSkin);
    } else if (m_skinPreviewLabel) {
        m_skinPreviewLabel->setPixmap(
            QPixmap::fromImage(m_previewSkin->getPreview())
                .scaled(m_skinPreviewLabel->size().isValid() ? m_skinPreviewLabel->size() : QSize(200, 200),
                        Qt::KeepAspectRatio, Qt::FastTransformation));
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
    if (m_websiteBtn)
        m_websiteBtn->setEnabled(enabled && !m_currentUuid.isEmpty() && m_client && !m_client->backendUrl().isEmpty());
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
        } else if (m_skinPreviewLabel) {
            m_skinPreviewLabel->setPixmap(
                QPixmap::fromImage(m_previewSkin->getPreview())
                    .scaled(m_skinPreviewLabel->size().isValid() ? m_skinPreviewLabel->size() : QSize(200, 200),
                            Qt::KeepAspectRatio, Qt::FastTransformation));
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
        } else if (m_skinPreviewLabel) {
            m_skinPreviewLabel->setPixmap(
                QPixmap::fromImage(m_previewSkin->getPreview())
                    .scaled(m_skinPreviewLabel->size().isValid() ? m_skinPreviewLabel->size() : QSize(200, 200),
                            Qt::KeepAspectRatio, Qt::FastTransformation));
        }
    }
}

// ─── Signal Handlers from XylarSkinClient ─────────────────────────────────────

void XylarSkinsPage::onPlayerRegistered(const QString& uuid, const QString& username, const QString& skinHash, const QString& skinModel)
{
    (void)username;
    m_currentUuid = uuid;
    m_currentSkinHash = skinHash;

    if (!skinHash.isEmpty()) {
        // Player has a skin: check cache or download
        if (m_client && m_client->hasCachedSkin(uuid)) {
            QByteArray cached = m_client->loadCachedSkin(uuid);
            // Verify hash matches
            if (XylarSkinClient::computeSha256(cached) == skinHash) {
                applySkinToPreview(cached, skinModel);
                m_ui->currentSkinLabel->setText(tr("Skin loaded from cache (hash matches)."));
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
                m_ui->currentSkinLabel->setText(tr("Skin loaded from cache."));
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

    m_ui->currentSkinLabel->setText(tr("Current skin: downloaded and cached."));
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
    Q_UNUSED(httpStatusCode)

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
