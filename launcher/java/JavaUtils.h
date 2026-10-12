#pragma once

#include <QProcess>
#include <QStringList>
#include "java/JavaInstall.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QString stripVariableEntries(QString name, QString target, QString remove);
QProcessEnvironment CleanEnviroment();
QStringList getMinecraftJavaBundle();
QStringList getXylarJavaBundle();

class JavaUtils : public QObject {
    Q_OBJECT
   public:
    JavaUtils();

    JavaInstallPtr MakeJavaPtr(QString path, QString id = "unknown", QString arch = "unknown");
    QList<QString> FindJavaPaths();
    JavaInstallPtr GetDefaultJava();

#ifdef Q_OS_WIN
    QList<JavaInstallPtr> FindJavaFromRegistryKey(DWORD keyType, QString keyName, QString keyJavaDir, QString subkeySuffix = "");
#endif

    static QString getJavaCheckPath();
    static const QString javaExecutable;
};
