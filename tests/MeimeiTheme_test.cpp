#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStackedWidget>
#include <QStyle>
#include <QTest>
class MeimeiThemeTest : public QObject {
    Q_OBJECT
   private slots:
    void characterImageIsLimitedToSelectedScenes()
    {
        const auto theme = QFINDTESTDATA("../community_themes/Meimei/themeStyle.css");
        QVERIFY(!theme.isEmpty());
        QDir::setSearchPaths("theme", { QFileInfo(theme).absolutePath() + "/resources" });
        QFile css(theme);
        QVERIFY(css.open(QIODevice::ReadOnly));
        qApp->setStyleSheet(QString::fromUtf8(css.readAll()));
        QStackedWidget pages;
        pages.setObjectName("mainPages");
        pages.resize(480, 270);
        pages.addWidget(new QWidget);
        pages.show();
        auto render = [&](bool character) {
            pages.setProperty("characterScene", character);
            pages.style()->unpolish(&pages);
            pages.style()->polish(&pages);
            QTest::qWait(5);
            return pages.grab().toImage();
        };
        const auto home = render(true), settings = render(false);
        QVERIFY(home != settings);
        QCOMPARE(render(false), settings);
        QCOMPARE(render(true), home);
        if (qEnvironmentVariableIsSet("XYLARJAVA_THEME_CAPTURE")) {
            home.save(qEnvironmentVariable("XYLARJAVA_THEME_CAPTURE") + "-home.png");
            settings.save(qEnvironmentVariable("XYLARJAVA_THEME_CAPTURE") + "-settings.png");
        }
        qApp->setStyleSheet({});
        QDir::setSearchPaths("theme", {});
    }
};
QTEST_MAIN(MeimeiThemeTest)
#include "MeimeiTheme_test.moc"
