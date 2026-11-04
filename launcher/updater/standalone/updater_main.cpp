#include "UpdaterApp.h"

#if defined Q_OS_WIN32
#include "console/WindowsConsole.h"
#endif

int main(int argc, char* argv[])
{
#if defined Q_OS_WIN32
    // attach the parent console if stdout not already captured
    console::WindowsConsoleGuard _consoleGuard;
#endif

    UpdaterApp wUpApp(argc, argv);

    switch (wUpApp.status()) {
        case UpdaterApp::Starting:
        case UpdaterApp::Initialized: {
            return wUpApp.exec();
        }
        case UpdaterApp::Failed:
            return 1;
        case UpdaterApp::Succeeded:
            return 0;
        default:
            return -1;
    }
}
