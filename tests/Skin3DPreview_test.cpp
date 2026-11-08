#include "ui/widgets/Skin3DPreview.h"
#include <QPainter>
#include <QTemporaryDir>
#include <QTest>
class Skin3DPreviewTest : public QObject {
    Q_OBJECT
   private slots:
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
