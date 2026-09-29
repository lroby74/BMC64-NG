#ifndef _kernel_h
#define _kernel_h

#include <circle_stdlib_app.h>

class CKernel : public CStdlibAppScreen
{
public:
    CKernel(void);

    TShutdownMode Run(void);

private:
    void test_vla_error(int size);
};

#endif
