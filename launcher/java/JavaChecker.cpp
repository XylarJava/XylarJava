#include "JavaChecker.h"

#include <QDebug>
#include <QFile>
#include <QMap>
#include <QProcess>

#include "Commandline.h"
#include "FileSystem.h"
#include "java/JavaUtils.h"

JavaChecker::JavaChecker(QString path, QString args, int minMem, int maxMem, int permGen, int id)
    : Task(), m_path(path), m_args(args), m_minMem(minMem), m_maxMem(maxMem), m_permGen(permGen), m_id(id)
{}

void JavaChecker::executeTask()
{
    QString checkerJar = JavaUtils::getJavaCheckPath();

    if (checkerJar.isEmpty()) {
        qDebug() << "Java checker library could not be found. Please check your installation.";
        return;
    }
#ifdef Q_OS_WIN
    checkerJar = FS::getPathNameInLocal8bit(checkerJar);
#endif

    QStringList args;

    process.reset(new QProcess());
    if (m_args.size()) {
        auto extraArgs = Commandline::splitArgs(m_args);
        args.append(extraArgs);
    }
    if (m_minMem != 0) {
        args << QString("-Xms%1m").arg(m_minMem);
    }
    if (m_maxMem != 0) {
        args << QString("-Xmx%1m").arg(m_maxMem);
    }
    if (m_permGen != 64 && m_permGen != 0) {
        args << QString("-XX:PermSize=%1m").arg(m_permGen);
    }

    args.append({ "-jar", checkerJar });
    process->setArguments(args);
    process->setProgram(m_path);
    process->setProcessChannelMode(QProcess::SeparateChannels);
    process->setProcessEnvironment(CleanEnviroment());
    qDebug() << "Running java checker:" << m_path << args.join(" ");

    connect(process.get(), &QProcess::finished, this, &JavaChecker::finished);
    connect(process.get(), &QProcess::errorOccurred, this, &JavaChecker::error);
    connect(process.get(), &QProcess::readyReadStandardOutput, this, &JavaChecker::stdoutReady);
    connect(process.get(), &QProcess::readyReadStandardError, this, &JavaChecker::stderrReady);
    connect(&killTimer, &QTimer::timeout, this, &JavaChecker::timeout);
    killTimer.setSingleShot(true);
    killTimer.start(15000);
    process->start();
}

void JavaChecker::stdoutReady()
{
    QByteArray data = process->readAllStandardOutput();
    QString added = QString::fromLocal8Bit(data);
    added.remove('\r');
    m_stdout += added;
}

void JavaChecker::stderrReady()
{
    QByteArray data = process->readAllStandardError();
    QString added = QString::fromLocal8Bit(data);
    added.remove('\r');
    m_stderr += added;
}

void JavaChecker::finished(int exitcode, QProcess::ExitStatus status)
{
    killTimer.stop();
    QProcessPtr _process = process;
    process.reset();

    Result result = {
        m_path,
        m_id,
    };
    result.errorLog = m_stderr;
    result.outLog = m_stdout;
    qDebug() << "STDOUT" << m_stdout;
    qWarning() << "STDERR" << m_stderr;
    qDebug() << "Java checker finished with status" << status << "exit code" << exitcode;

    if (status == QProcess::CrashExit || exitcode == 1) {
        result.validity = Result::Validity::Errored;
        emit checkFinished(result);
        emitSucceeded();
        return;
    }

    bool success = true;

    QMap<QString, QString> results;

    QStringList lines = m_stdout.split("\n", Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        // NOTE: workaround for GH-4125, where garbage is getting printed into stdout on bedrock linux
        if (line.contains("/bedrock/strata")) {
            continue;
        }

        auto parts = line.split('=', Qt::SkipEmptyParts);
        if (parts.size() != 2 || parts[0].isEmpty() || parts[1].isEmpty()) {
            continue;
        } else {
            results.insert(parts[0], parts[1]);
        }
    }

    if (!results.contains("os.arch") || !results.contains("java.version") || !results.contains("java.vendor") || !success) {
        result.validity = Result::Validity::ReturnedInvalidData;
        emit checkFinished(result);
        emitSucceeded();
        return;
    }

    auto os_arch = results["os.arch"];
    auto java_version = results["java.version"];
    auto java_vendor = results["java.vendor"];
    bool is_64 = os_arch == "x86_64" || os_arch == "amd64" || os_arch == "aarch64" || os_arch == "arm64" || os_arch == "riscv64" || os_arch == "ppc64le" || os_arch == "ppc64";

    result.validity = Result::Validity::Valid;
    result.is_64bit = is_64;
    result.mojangPlatform = is_64 ? "64" : "32";
    result.realPlatform = os_arch;
    result.javaVersion = java_version;
    result.javaVendor = java_vendor;
    qDebug() << "Java checker succeeded.";
    emit checkFinished(result);
    emitSucceeded();
}

void JavaChecker::error(QProcess::ProcessError err)
{
    if (err == QProcess::FailedToStart) {
        // Only what explains a Java that won't start: the whole environment can hold tokens and
        // passwords, and logs get shared when asking for help.
        const QProcessEnvironment env = process->processEnvironment();
        qDebug() << "Java checker has failed to start:" << m_path;
        qDebug() << "PATH:" << env.value(QStringLiteral("PATH")) << "JAVA_HOME:" << env.value(QStringLiteral("JAVA_HOME"));
        killTimer.stop();
        emit checkFinished({ m_path, m_id });
    }
    emitSucceeded();
}

void JavaChecker::timeout()
{
    // NO MERCY. NO ABUSE.
    if (process) {
        qDebug() << "Java checker has been killed by timeout.";
        process->kill();
    }
}
