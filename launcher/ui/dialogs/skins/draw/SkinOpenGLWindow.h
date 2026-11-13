#pragma once

#include <QMatrix4x4>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLWindow>
#include <QVector2D>
#include "minecraft/skins/SkinModel.h"
#include "ui/dialogs/skins/draw/BoxGeometry.h"
#include "ui/dialogs/skins/draw/Scene.h"

class QWidget;

class SkinProvider {
   public:
    virtual ~SkinProvider() = default;
    virtual SkinModel* getSelectedSkin() = 0;
    virtual QHash<QString, QImage> capes() = 0;
};
class SkinOpenGLWindow : public QOpenGLWindow, protected QOpenGLFunctions {
    Q_OBJECT

   public:
    SkinOpenGLWindow(SkinProvider* parent, QColor color);
    virtual ~SkinOpenGLWindow();

    void updateScene(SkinModel* skin);
    void updateCape(const QImage& cape);
    void setElytraVisible(bool visible);

    static bool hasOpenGL();
    QWidget* createViewControls(QWidget* parent);
    void resetView();
    void showFront();
    void showBack();

   protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void initShaders();

    void generateBackgroundTexture(int width, int height, int tileSize);
    void renderBackground();

   private:
    QOpenGLShaderProgram* m_modelProgram = nullptr;
    QOpenGLShaderProgram* m_backgroundProgram = nullptr;
    opengl::Scene* m_scene = nullptr;

    QMatrix4x4 m_projection;

    QVector2D m_mousePosition;

    bool m_isMousePressed = false;
    float m_distance = 48;
    float m_yaw = 90;   // Horizontal rotation angle
    float m_pitch = 0;  // Vertical rotation angle

    bool m_isFirstFrame = true;

    opengl::BoxGeometry* m_background = nullptr;
    QOpenGLTexture* m_backgroundTexture = nullptr;
    QColor m_baseColor;
    SkinProvider* m_parent = nullptr;
};
