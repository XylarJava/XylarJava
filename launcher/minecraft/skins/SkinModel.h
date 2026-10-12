#pragma once

#include <QDir>
#include <QImage>
#include <QJsonObject>

class SkinModel {
   public:
    enum Model { CLASSIC, SLIM };

    SkinModel() = default;
    SkinModel(QString path);
    SkinModel(QDir skinDir, QJsonObject obj);
    SkinModel(const QImage& texture, Model model = CLASSIC);
    virtual ~SkinModel() = default;

    static QImage defaultSteveTexture();
    static QImage defaultAlexTexture();

    QString name() const;
    QString getModelString() const;
    bool isValid() const;
    QString getPath() const { return m_path; }
    QImage getTexture() const { return m_texture; }
    QImage getPreview() const { return m_preview; }
    QString getCapeId() const { return m_capeId; }
    Model getModel() const { return m_model; }
    QString getURL() const { return m_url; }

    bool rename(QString newName);
    void setCapeId(QString capeID) { m_capeId = capeID; }
    void setModel(Model model);
    void setTexture(const QImage& texture);
    void setURL(QString url) { m_url = url; }
    void refresh();

    QJsonObject toJSON() const;

   private:
    QString m_path;
    QImage m_texture;
    QImage m_preview;
    QString m_capeId;
    Model m_model;
    QString m_url;
};
