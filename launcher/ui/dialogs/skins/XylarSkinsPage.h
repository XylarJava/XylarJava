#pragma once

#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QString>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/draw/SkinOpenGLWindow.h"

class XylarSkinClient;

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
    void onSiteLinkReady(const QString& uuid, const QString& code);

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
};
