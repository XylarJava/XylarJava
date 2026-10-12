#include <iostream>

#include "Application.h"

#if defined Q_OS_WIN32
#include "console/WindowsConsole.h"
#endif

int main(int argc, char* argv[])
{
#if defined Q_OS_WIN32
    // used on Windows to attach the standard IO streams
    console::WindowsConsoleGuard _consoleGuard;
#endif

    // initialize Qt
    Application app(argc, argv);
    switch (app.status()) {
        case Application::StartingUp:
        case Application::Initialized: {
            Q_INIT_RESOURCE(multimc);
            Q_INIT_RESOURCE(documents);
            Q_INIT_RESOURCE(XylarJava);

            Q_INIT_RESOURCE(pe_dark);
            Q_INIT_RESOURCE(pe_light);
            Q_INIT_RESOURCE(pe_blue);
            Q_INIT_RESOURCE(pe_colored);
            Q_INIT_RESOURCE(breeze_dark);
            Q_INIT_RESOURCE(breeze_light);
            Q_INIT_RESOURCE(OSX);
            Q_INIT_RESOURCE(iOS);
            Q_INIT_RESOURCE(flat);
            Q_INIT_RESOURCE(flat_white);

            Q_INIT_RESOURCE(shaders);
            return app.exec();
        }
        case Application::Failed:
            return 1;
        case Application::Succeeded:
            return 0;
        default:
            return -1;
    }
}
