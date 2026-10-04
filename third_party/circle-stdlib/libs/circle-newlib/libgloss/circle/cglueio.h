#ifndef _CIRCNEWLIB_CGLUEIO_H
#define _CIRCNEWLIB_CGLUEIO_H

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <errno.h>
#include <assert.h>

namespace _CircleStdlib
{
    class CGlueIO
    {
    public:
        CGlueIO() : mRefCount(1)
        {
        }

        virtual
        ~CGlueIO ()
        {
        }

        virtual int
        Read (void *pBuffer, int nCount) = 0;

        virtual int
        Write (const void *pBuffer, int nCount) = 0;

        virtual int
        LSeek(int ptr, int dir) = 0;

        virtual int
        Close (void) = 0;

        virtual int
        FTruncate (off_t)
        {
            errno = EINVAL;
            return -1;
        }

        virtual int
        FSync (void)
        {
            errno = EINVAL;
            return -1;
        }

        virtual int
        Fcntl (int cmd, int arg)
        {
            errno = ENOSYS;
            return -1;
        }

        virtual int
        FStat (struct stat *buf) = 0;

        virtual int
        IsATty (void) = 0;

        void IncrementRefCount (void)
        {
            mRefCount += 1;
        }

        void DecrementRefCount (void)
        {
            assert (mRefCount > 0);
            mRefCount -= 1;
        }

        unsigned int GetRefCount (void) const
        {
            return mRefCount;
        }

        /**
         * Arbitrary device id values to identify the type of device
         * via fstat().
         */
        enum DeviceId {
            DeviceIdFatFs = 0x0101,
            DeviceIdConsole = 0x0202,
            DeviceIdSocket = 0x0303
        };

        /**
         * Status flags for GetStatus()
         * 
         * Extends the corresponding struct in netconnection.h
         * for all file descriptor types.
         */
        struct TStatus
        {
            bool bConnected;
            bool bRxReady;
            bool bTxReady;
            bool bException;
        };
        
        virtual TStatus GetSelectStatus (void) const = 0;

    private:
        unsigned int mRefCount;
    };
}

#endif

