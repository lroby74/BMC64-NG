#include "circle_macros.h"

#include <circle/sched/pipe.h>
#include <circle/sched/scheduler.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include "cglueio.h"
#include "filetable.h"

namespace _CircleStdlib
{
    class CGlueIoPipeFile : public CGlueIO
    {
    public:
        CGlueIoPipeFile(CPipeFile *pPipeFile, int flags)
            : mPipeFile(pPipeFile), mFlags(flags)
        {
            assert(mPipeFile);
            if (mFlags & O_NONBLOCK)
            {
                mPipeFile->SetBlocking(FALSE);
            }
            else
            {
                mPipeFile->SetBlocking(TRUE);
            }
        }

        ~CGlueIoPipeFile()
        {
        }

        int Read(void *pBuffer, int nCount) override
        {
            int nResult = mPipeFile->Read(pBuffer, static_cast<size_t>(nCount));
            
            if (nResult < 0)
            {
                if (nResult == -CPipeFile::WouldBlock)
                {
                    errno = EWOULDBLOCK;
                }
                else
                {
                    errno = EIO;
                }
                nResult = -1;
            }
            
            CScheduler::Get()->Yield();
            return nResult;
        }

        int Write(const void *pBuffer, int nCount) override
        {
            int nResult = mPipeFile->Write(pBuffer, static_cast<size_t>(nCount));
            
            if (nResult < 0)
            {
                if (nResult == -CPipeFile::WouldBlock)
                {
                    errno = EWOULDBLOCK;
                }
                else if (nResult == -CPipeFile::NoReader)
                {
                    errno = EPIPE;
                }
                else
                {
                    errno = EIO;
                }
                nResult = -1;
            }
            
            CScheduler::Get()->Yield();
            return nResult;
        }

        int LSeek(int ptr, int dir) override
        {
            errno = ESPIPE;
            return -1;
        }

        int Close(void) override
        {
            mPipeFile->Close();
            CScheduler::Get()->Yield();
            return 0;
        }

        int FStat(struct stat *buf) override
        {
            assert(buf);
            memset(buf, 0, sizeof(*buf));
            buf->st_dev = 0x0404; // Pipe device ID equivalent
            buf->st_ino = 3000;
            buf->st_nlink = 1;
            buf->st_mode = S_IRUSR | S_IWUSR | S_IFIFO;
            return 0;
        }

        int IsATty(void) override
        {
            errno = ENOTTY;
            return 0;
        }

        TStatus GetSelectStatus(void) const override
        {
            CPipeFile::TStatus status = mPipeFile->GetStatus();
            return { true, status.bReadReady, status.bWriteReady, status.bException };
        }

        int Fcntl(int cmd, int arg) override
        {
            if (cmd == F_GETFL)
            {
                return mFlags;
            }
            else if (cmd == F_SETFL)
            {
                mFlags = arg;
                if (mFlags & O_NONBLOCK)
                {
                    mPipeFile->SetBlocking(FALSE);
                }
                else
                {
                    mPipeFile->SetBlocking(TRUE);
                }
                return 0;
            }
            errno = EINVAL;
            return -1;
        }

    private:
        CPipeFile *mPipeFile;
        int mFlags;
    };
}

extern "C" int pipe2(int fildes[2], int flags)
{
    if (!fildes)
    {
        errno = EFAULT;
        return -1;
    }

    if (flags & ~(O_CLOEXEC | O_NONBLOCK))
    {
        errno = EINVAL;
        return -1;
    }

    int slotRead = -1;
    int slotWrite = -1;

    {
        _CircleStdlib::FileTable::FileTableLock fileTabLock;

        _CircleStdlib::CircleFile *circleFileRead = nullptr;
        slotRead = _CircleStdlib::FileTable::FindFreeFileSlot(circleFileRead);
        
        if (slotRead == -1)
        {
            errno = EMFILE;
            return -1;
        }

        _CircleStdlib::CircleFile *circleFileWrite = nullptr;
        slotWrite = _CircleStdlib::FileTable::FindFreeFileSlot(circleFileWrite, slotRead + 1);
        
        if (slotWrite == -1)
        {
            errno = EMFILE;
            return -1;
        }

        assert(circleFileRead != nullptr);
        assert(circleFileWrite != nullptr);

        /*
         * Note: CPipe automatically deletes itself when both the reader
         * and writer CPipeFile objects are closed. Therefore, we do not
         * need to store the CPipe pointer or explicitly delete it.
         */
        CPipe *pPipe = new CPipe; 

        auto const newReader = new _CircleStdlib::CGlueIoPipeFile(pPipe->GetReader(), flags);
        circleFileRead->AssignGlueIO(*newReader);

        auto const newWriter = new _CircleStdlib::CGlueIoPipeFile(pPipe->GetWriter(), flags);
        circleFileWrite->AssignGlueIO(*newWriter);
    }
    
    fildes[0] = slotRead;
    fildes[1] = slotWrite;

    return 0;
}

extern "C" int pipe(int fildes[2])
{
    return pipe2(fildes, 0);
}
