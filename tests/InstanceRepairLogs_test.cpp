#include <QTest>
#include "InstanceRepair.h"
class InstanceRepairLogsTest : public QObject {
    Q_OBJECT
   private slots:
    void identifiesExternalFailures_data()
    {
        QTest::addColumn<QString>("log");
        QTest::addColumn<QString>("cause");
        QTest::newRow("antivirus")
            << QStringLiteral("Operation did not complete successfully because the file contains a virus or potentially unwanted software.")
            << QStringLiteral("Windows security");
        QTest::newRow("path") << QStringLiteral("CreateProcess error=206, filename or extension is too long")
                              << QStringLiteral("Windows path");
        QTest::newRow("arch") << QStringLiteral("CreateProcess error=193, not a valid Win32 application")
                              << QStringLiteral("CPU architecture");
        QTest::newRow("disk") << QStringLiteral("java.io.IOException: No space left on device") << QStringLiteral("disk is full");
        QTest::newRow("driver") << QStringLiteral("# Problematic frame:\n# C  [atio6axx.dll+0x1ff]") << QStringLiteral("graphics driver");
        QTest::newRow("native-memory") << QStringLiteral("java.lang.OutOfMemoryError: unable to create native thread")
                                       << QStringLiteral("native threads");
        QTest::newRow("metaspace") << QStringLiteral("java.lang.OutOfMemoryError: Metaspace") << QStringLiteral("class metadata");
        QTest::newRow("certificate") << QStringLiteral("javax.net.ssl.SSLHandshakeException: PKIX path building failed")
                                     << QStringLiteral("certificate");
        QTest::newRow("permissions") << QStringLiteral("java.nio.file.AccessDeniedException: C:\\Games\\mods\\test.jar")
                                     << QStringLiteral("denied access");
    }
    void identifiesExternalFailures()
    {
        QFETCH(QString, log);
        QFETCH(QString, cause);
        const auto d = InstanceRepair::diagnoseLog(log);
        QVERIFY2(d.cause.contains(cause), qPrintable(d.cause));
        QVERIFY(d.blocking);
        QVERIFY(!d.files);
        QVERIFY(d.modsToDisable.isEmpty());
        QVERIFY(!d.advice.isEmpty());
    }
    void leavesUnidentifiedCrashUnidentified()
    {
        const auto d = InstanceRepair::diagnoseLog(QStringLiteral("java.lang.RuntimeException: unknown mod failure"));
        QVERIFY(d.cause.isEmpty());
        QVERIFY(!d.blocking);
        QVERIFY(!d.files);
        QVERIFY(!d.evidence.isEmpty());
    }
    void redactsSecretsFromEvidence()
    {
        const auto d = InstanceRepair::diagnoseLog(
            QStringLiteral("Exception: accessToken=secret-value Authorization: Bearer other-secret X-Player-Key: another-secret"));
        const auto text = d.evidence.join('\n');
        QVERIFY(!text.contains("secret-value"));
        QVERIFY(!text.contains("other-secret"));
        QVERIFY(!text.contains("another-secret"));
    }
};
QTEST_GUILESS_MAIN(InstanceRepairLogsTest)
#include "InstanceRepairLogs_test.moc"
