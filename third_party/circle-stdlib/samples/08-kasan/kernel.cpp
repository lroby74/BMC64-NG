#include "kernel.h"

CKernel::CKernel(void) : CStdlibAppScreen("08-kasan")
{
    mActLED.Blink(5); // show we are alive
}

void CKernel::test_vla_error(int size)
{
    mLogger.Write(GetKernelName(), LogNotice, "Allocating VLA of size %d",
                  size);

    volatile char buffer[size];

    mLogger.Write(GetKernelName(), LogNotice, "Buffer address: 0x%lX", (unsigned long)buffer);

    mLogger.Write(GetKernelName(), LogNotice,
                  "Writing out of bounds to trigger KASan right redzone error");

    // Intentionally write out of bounds by 1 byte to trigger right redzone
    // access violation
    buffer[size] = 'x';
}

CStdlibApp::TShutdownMode CKernel::Run(void)
{
    mLogger.Initialize(&mSerial);
    mLogger.Write(GetKernelName(), LogNotice, "KASan Alloca Test");

    mLogger.Write(GetKernelName(), LogNotice, "Testing Heap Overflow...");
    char *heap_buf = new char[16];
    heap_buf[16] = 'x'; // Trigger KASan heap buffer overflow

    test_vla_error(16);

    mLogger.Write(GetKernelName(), LogNotice,
                  "KASan Alloca Test finished (should not be reached!)");

    return ShutdownHalt;
}
