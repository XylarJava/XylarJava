#pragma once

#include <QtCore>

#include <QApplication>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QFlag>
#include <QIcon>
#include <QLocalSocket>
#include <QUrl>
#include <memory>

#define XYLAR_EXTERNAL_EXE
#include "FileSystem.h"

class FileLinkApp : public QCoreApplication {
    Q_OBJECT
   public:
    enum Status { Starting, Failed, Succeeded, Initialized };
    FileLinkApp(int& argc, char** argv);
    virtual ~FileLinkApp();
    Status status() const { return m_status; }

   private:
    void joinServer(QString server);
    void readPathPairs();
    void runLink();
    void sendResults();

    Status m_status = Status::Starting;

    bool m_useHardLinks = false;

    QDateTime m_startTime;
    QLocalSocket socket;
    QDataStream in;
    quint32 blockSize;

    QList<FS::LinkPair> m_links_to_make;
    QList<FS::LinkResult> m_path_results;

};
