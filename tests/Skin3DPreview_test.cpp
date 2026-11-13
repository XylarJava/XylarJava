#include "ui/widgets/Skin3DPreview.h"
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>
#include <QToolButton>
#include "minecraft/skins/SkinModel.h"
class Skin3DPreviewTest : public QObject {
    Q_OBJECT
   private slots:
    void normalizesLegacyAndHdTexturesWithoutLosingEdges()
    {
        QImage legacy(64, 32, QImage::Format_ARGB32);
        legacy.fill(Qt::transparent);
        legacy.setPixelColor(4, 20, Qt::red);
        legacy.setPixelColor(7, 20, Qt::blue);
        auto modern = SkinModel::normalizeTexture(legacy);
        QCOMPARE(modern.size(), QSize(64, 64));
        QCOMPARE(modern.pixelColor(20, 52), QColor(Qt::blue));
        QCOMPARE(modern.pixelColor(23, 52), QColor(Qt::red));
        QCOMPARE(modern.pixelColor(31, 15).alpha(), 255);
        QCOMPARE(modern.pixelColor(63, 31).alpha(), 255);
        QCOMPARE(modern.pixelColor(63, 63).alpha(), 0);
        auto hd = SkinModel::normalizeTexture(legacy.scaled(128, 64, Qt::IgnoreAspectRatio, Qt::FastTransformation));
        QCOMPARE(hd.size(), QSize(128, 128));
        QCOMPARE(hd.pixelColor(40, 104), QColor(Qt::blue));
        QCOMPARE(hd.pixelColor(47, 104), QColor(Qt::red));
        QVERIFY(SkinModel::normalizeTexture(QImage(65, 65, QImage::Format_ARGB32)).isNull());
    }
    void backViewShowsTheCapeAndKeyboardAndPanCanBeReset()
    {
        QImage skin(64, 64, QImage::Format_ARGB32);
        skin.fill(Qt::transparent);
        QImage cape(64, 32, QImage::Format_ARGB32);
        cape.fill(Qt::transparent);
        {
            QPainter p(&cape);
            p.fillRect(QRect(1, 1, 10, 16), Qt::magenta);
        }
        Skin3DPreview preview;
        preview.resize(300, 400);
        preview.setSkin(skin, false);
        preview.setCape(cape);
        preview.show();
        QTest::qWait(20);
        auto* back = preview.findChild<QToolButton*>(QStringLiteral("skinBackButton"));
        QVERIFY(back);
        QTest::mouseClick(back, Qt::LeftButton);
        auto image = preview.grab().toImage();
        int pixels = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const auto c = image.pixelColor(x, y);
                if (c.red() > 100 && c.blue() > 100 && c.green() < 40)
                    ++pixels;
            }
        QVERIFY(pixels > 500);
        preview.resetView();
        const auto original = preview.grab().toImage();
        QTest::keyClick(&preview, Qt::Key_Left);
        QVERIFY(original != preview.grab().toImage());
        QTest::keyClick(&preview, Qt::Key_R);
        QCOMPARE(original, preview.grab().toImage());
        QTest::mousePress(&preview, Qt::RightButton, Qt::NoModifier, QPoint(130, 150));
        QTest::mouseMove(&preview, QPoint(180, 180));
        QTest::mouseRelease(&preview, Qt::RightButton, Qt::NoModifier, QPoint(180, 180));
        QVERIFY(original != preview.grab().toImage());
        preview.resetView();
    }
    void rendersAndRotatesWithoutOpenGL()
    {
        QImage skin(64, 64, QImage::Format_ARGB32);
        skin.fill(Qt::transparent);
        {
            QPainter p(&skin);
            p.fillRect(QRect(0, 0, 32, 16), Qt::red);
            p.fillRect(QRect(16, 16, 24, 16), Qt::green);
            p.fillRect(QRect(0, 16, 16, 16), Qt::yellow);
            p.fillRect(QRect(40, 16, 16, 16), Qt::blue);
            p.fillRect(QRect(16, 48, 32, 16), Qt::cyan);
        }
        Skin3DPreview preview;
        preview.resize(300, 400);
        preview.setSkin(skin, false);
        preview.show();
        QTest::qWait(20);
        const auto front = preview.grab().toImage();
        int red = 0, green = 0;
        for (int y = 0; y < front.height(); ++y)
            for (int x = 0; x < front.width(); ++x) {
                const auto c = front.pixelColor(x, y);
                if (c.red() > 150 && c.green() < 50)
                    ++red;
                if (c.green() > 80 && c.red() < 50)
                    ++green;
            }
        QVERIFY(red > 100);
        QVERIFY(green > 100);
        QTest::mousePress(&preview, Qt::LeftButton, Qt::NoModifier, QPoint(130, 150));
        QTest::mouseMove(&preview, QPoint(260, 200));
        QTest::mouseRelease(&preview, Qt::LeftButton, Qt::NoModifier, QPoint(260, 200));
        QVERIFY(front != preview.grab().toImage());
        QTest::mouseDClick(&preview, Qt::LeftButton, Qt::NoModifier, QPoint(130, 150));
        QCOMPARE(front, preview.grab().toImage());
        if (qEnvironmentVariableIsSet("XYLARJAVA_PREVIEW_CAPTURE"))
            front.save(qEnvironmentVariable("XYLARJAVA_PREVIEW_CAPTURE"));
    }
};
QTEST_MAIN(Skin3DPreviewTest)
#include "Skin3DPreview_test.moc"
