#pragma once

#include <QDialog>
#include <QImage>

class QGridLayout;
class QLabel;
class QVBoxLayout;
class ProfileClient;
class SkinPreview;

/**
 * A player's XylarJava profile: their skin, how long they've played, when and on which
 * version, and the server they play on most. Opened from the Social panel, for yourself
 * or a friend.
 */
class PlayerProfileDialog : public QDialog {
    Q_OBJECT

   public:
    PlayerProfileDialog(ProfileClient* client, const QString& name, const QImage& skin, QWidget* parent = nullptr);

    /// A skin that arrived after the dialog opened.
    void setSkin(const QImage& skin);

   private:
    void load();
    void showMessage(const QString& text);

    ProfileClient* m_client = nullptr;
    QString m_name;
    SkinPreview* m_skin = nullptr;
    QLabel* m_title = nullptr;
    QLabel* m_since = nullptr;
    QLabel* m_message = nullptr;
    QWidget* m_details = nullptr;
    QGridLayout* m_stats = nullptr;
    QVBoxLayout* m_versions = nullptr;
};
