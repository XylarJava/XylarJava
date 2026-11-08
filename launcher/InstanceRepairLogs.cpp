#include <QObject>
#include <QRegularExpression>
#include "InstanceRepair.h"

namespace InstanceRepair {
Diagnosis diagnoseLog(const QString& log)
{
    Diagnosis d;
    static const QRegularExpression important(
        QStringLiteral(
            R"(Caused by:|Exception|\berror\b|failed|Problematic frame|EXCEPTION_ACCESS_VIOLATION|virus|unwanted software|0xC000|GLFW|OutOfMemory|No space|disk.*full)"),
        QRegularExpression::CaseInsensitiveOption);
    const auto lines = log.split(QLatin1Char('\n'));
    for (const auto& line : lines) {
        if (!important.match(line).hasMatch())
            continue;
        QString safe = line.left(600).trimmed();
        safe.replace(QRegularExpression(
                         QStringLiteral(R"((Bearer\s+|(?:access[_-]?token|refresh[_-]?token|clientToken|X-Player-Key)["\s:=]+)[^\s,;"']+)"),
                         QRegularExpression::CaseInsensitiveOption),
                     QStringLiteral("\\1[redacted]"));
        if (!d.evidence.contains(safe))
            d.evidence << safe;
        if (d.evidence.size() == 8)
            break;
    }
    auto has = [&](const QString& pattern) {
        return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption)
            .match(log)
            .hasMatch();
    };
    if (has(QStringLiteral(
            R"(contains a virus|potentially unwanted software|ERROR_VIRUS_(?:INFECTED|DELETED)|Win32(?:Exception|Error)[^\n]*(?:225|226)\b)"))) {
        d.cause = QObject::tr("Windows security blocked a launcher or Java file.");
        d.advice << QObject::tr(
            "Check Windows Security → Protection history for the detection and affected file. Download the current official release and "
            "report its detection name and SHA-256. Keep antivirus protection enabled.");
        d.blocking = true;
    } else if (has(QStringLiteral(
                   R"(No space left on device|not enough space on the disk|disk is full|ERROR_DISK_FULL|error[=: ]+112\b)"))) {
        d.cause = QObject::tr("The disk is full.");
        d.advice << QObject::tr("Free space on the game and temporary-file disks before trying again.");
        d.blocking = true;
    } else if (has(QStringLiteral(R"(CreateProcess error=206|filename or extension is too long|ERROR_FILENAME_EXCED_RANGE)"))) {
        d.cause = QObject::tr("A Windows path or launch command exceeds the operating system limit.");
        d.advice << QObject::tr(
            "Move the launcher data and instance to a shorter path, such as C:\\Games\\XylarJava, then retry. Keep a backup before moving "
            "the instance.");
        d.blocking = true;
    } else if (has(QStringLiteral(
                   R"(CreateProcess error=193|not a valid Win32 application|ERROR_BAD_EXE_FORMAT|wrong ELF class|Bad CPU type)"))) {
        d.cause = QObject::tr("Java or a native library was built for a different CPU architecture.");
        d.advice << QObject::tr(
            "Select a Java runtime and launcher build matching your Windows/Linux architecture. An x64 game requires an x64 runtime.");
        d.blocking = true;
    } else if (has(QStringLiteral(R"(VCRUNTIME\d*[^\n]*\.dll|MSVCP\d*[^\n]*\.dll|0xC0000135|3221225781)"))) {
        d.cause = QObject::tr("Windows could not load a required runtime DLL.");
        d.advice << QObject::tr(
            "Check the missing DLL in the log. For VCRUNTIME/MSVCP, repair the Microsoft Visual C++ Redistributable from Microsoft; for "
            "launcher DLLs, reinstall the official package. Never download individual DLLs from third-party sites.");
        d.blocking = true;
    } else if (has(QStringLiteral(R"((?:Problematic frame|^# C)[^\n]*(?:atio\w*|nvoglv\w*|ig\w*icd\w*)\.dll)"))) {
        d.cause = QObject::tr("The game crashed inside the graphics driver.");
        d.advice << QObject::tr(
            "Update the graphics driver from your GPU vendor. A native driver crash cannot be fixed by downloading the game again.");
        d.blocking = true;
    } else if (has(QStringLiteral(
                   R"(java\.lang\.OutOfMemoryError:\s*(?:Metaspace|Compressed class space|unable to create native thread))"))) {
        d.cause = QObject::tr("Java exhausted native threads or class metadata memory, rather than the game heap.");
        d.advice << QObject::tr(
            "Close other programs and check custom MaxMetaspaceSize, MaxDirectMemorySize or thread-limit options. Increasing the game heap "
            "can make this problem worse.");
        d.blocking = true;
    } else if (has(QStringLiteral(
                   R"(PKIX path building failed|unable to find valid certification path|certificate has expired|SSLHandshakeException)"))) {
        d.cause = QObject::tr("A secure connection failed certificate validation.");
        d.advice << QObject::tr(
            "Check the computer clock, update Java and check proxy/certificate configuration. Keep TLS verification enabled.");
        d.blocking = true;
    } else if (has(QStringLiteral(R"(Invalid session|Failed to verify username|Authentication servers are down|Token expired)"))) {
        d.cause = QObject::tr("The account session or authentication service rejected the connection.");
        d.advice << QObject::tr(
            "Refresh the selected account with its provider. If the provider is unavailable, wait and retry. Reinstalling mods does not "
            "repair account sessions.");
        d.blocking = true;
    } else if (has(QStringLiteral(R"(CreateProcess error=5|AccessDeniedException|Permission denied|Access is denied)"))) {
        d.cause = QObject::tr("The operating system denied access to a required file.");
        d.advice << QObject::tr(
            "Check the exact file in the log, its folder permissions and Windows Controlled Folder Access. Close programs locking the "
            "file, and keep launcher data in a writable user folder.");
        d.blocking = true;
    } else if (has(QStringLiteral(R"(UnknownHostException|Could not resolve host|Network is unreachable|Connection timed out)"))) {
        d.cause = QObject::tr("A required network service could not be reached.");
        d.advice << QObject::tr("Check the failing hostname, DNS and proxy settings, then retry. Existing game files will be kept.");
        d.blocking = true;
    }
    return d;
}
}  // namespace InstanceRepair
