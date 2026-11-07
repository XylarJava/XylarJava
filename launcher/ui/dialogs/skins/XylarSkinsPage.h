#pragma once

#include <QDialog>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QJsonArray>
#include <QLabel>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QString>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"

class XylarSkinClient;
class QListWidgetItem;

namespace Ui {
class XylarSkinsPage;
}

class XylarSkinsPage : public QDialog, public SkinProvider {
    Q_OBJECT

   public:
    explicit XylarSkinsPage(QWidget* parent, MinecraftAccountPtr acct);
    ~XylarSkinsPage() override;

    // SkinProvider interface
    SkinModel* getSelectedSkin() override;
    QHash<QString, QImage> capes() override;

   private slots:
    void on_selectFileBtn_clicked();
    void on_uploadBtn_clicked();
    void on_deleteBtn_clicked();
    void on_steveBtn_toggled(bool checked);
    void on_alexBtn_toggled(bool checked);

    // XylarSkinClient signal handlers
    void onPlayerRegistered(const QString& uuid, const QString& username, const QString& skinHash, const QString& skinModel);
    void onPlayerFetched(const QString& uuid, const QString& username, const QString& skinHash, const QString& skinModel);
    void onSkinMetadataReceived(const QString& uuid, bool hasSkin, const QString& skinHash, const QString& model);
    void onSkinDownloaded(const QString& uuid, const QString& localPath, const QString& model, const QString& skinHash);
    void onSkinUploaded(const QString& uuid, const QString& skinHash, const QString& model);
    void onSkinDeleted(const QString& uuid);
    void onRequestFailed(const QString& action, const QString& errorMessage, int httpStatusCode);
    void manageOnWebsite();
    void importSkinAccess();
    void exportSkinAccess();
    void onSiteLinkReady(const QString& uuid, const QString& code);

    // Capes
    void on_uploadCapeBtn_clicked();
    void onCapeItemClicked(QListWidgetItem* item);
    void onCapesListed(const QJsonArray& capes);
    void onCapeMetadataReceived(const QString& uuid, bool hasCape, const QString& capeHash, const QString& capeId);
    void onCapeDownloaded(const QString& hash, const QByteArray& pngData);
    void onCapeChanged(const QString& uuid, const QString& capeHash, const QString& capeId);

   private:
    void setupSkinClient();
    void setupPreviewWidget();
    void loadCurrentSkin();
    void refreshFromCache();
    void applySkinToPreview(const QByteArray& pngData, const QString& model);
    void restoreAccountSkinPreview();
    void setStatusMessage(const QString& msg, bool isError = false);
    void setUIEnabled(bool enabled);
    QString currentModel() const;
    QString xylarUuid() const;
    void setupCapeList();
    void requestCapes();
    void addCapeItem(const QString& id, const QString& name, const QString& hash);
    void removeCapeItem(const QString& id);
    void selectCurrentCape();
    void showCapeInPreview();
    /// The front/back picture used without OpenGL, with the cape on the back.
    void refreshFlatPreview();
    QIcon capeIcon(const QImage& cape) const;

   private:
    Ui::XylarSkinsPage* m_ui;
    MinecraftAccountPtr m_acct;
    XylarSkinClient* m_client = nullptr;
    bool m_ownsClient = false;

    QString m_currentUuid;
    QString m_currentSkinHash;
    QString m_selectedFilePath;

    SkinModel* m_previewSkin = nullptr;
    SkinOpenGLWindow* m_skinPreview = nullptr;
    QLabel* m_skinPreviewLabel = nullptr;
    QPushButton* m_websiteBtn = nullptr;
    QPushButton* m_accessButton = nullptr;
    bool m_uiEnabled = true;

    // The cape worn (empty hash: none), and the cape textures known so far, by hash.
    QString m_currentCapeHash;
    QString m_currentCapeId;
    bool m_capesRequested = false;
    QHash<QString, QImage> m_capeImages;
};
