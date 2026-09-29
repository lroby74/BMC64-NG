#include "kernel.h"

#include <circle/startup.h>

int main(void)
{
    CKernel kernel;
    if (!kernel.Initialize())
    {
        halt();
        return EXIT_HALT;
    }

    CStdlibApp::TShutdownMode shutdown_mode = kernel.Run();

    kernel.Cleanup();

    switch (shutdown_mode)
    {
    case CStdlibApp::ShutdownReboot:
        reboot();
        return EXIT_REBOOT;

    case CStdlibApp::ShutdownHalt:
    default:
        halt();
        return EXIT_HALT;
    }
}
