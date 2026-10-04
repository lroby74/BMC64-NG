#ifndef _CIRCNEWLIB_FILETABLE_H
#define _CIRCNEWLIB_FILETABLE_H

#include <sys/dirent.h>
#include <circle/spinlock.h>
#include "circle_glue.h"
#include <assert.h>

struct _CIRCLE_DIR
{
    _CIRCLE_DIR()
        : mFirstRead(0), mOpen(0)
    {
        mEntry.d_ino = 0;
        mEntry.d_name[0] = 0;
    }

    FATFS_DIR mCurrentEntry;
    struct dirent mEntry;
    unsigned int mFirstRead : 1;
    unsigned int mOpen : 1;
};

namespace _CircleStdlib
{
    class CGlueIO;

    class CircleFile
    {
    public:
        CircleFile()
            : mCGlueIO(nullptr)
        {
        }

        bool IsOpen(void) const
        {
            return mCGlueIO != nullptr;
        }

        void AssignGlueIO(CGlueIO &glueIO)
        {
            assert(!mCGlueIO);
            mCGlueIO = &glueIO;
        }

        void CloseGlueIO(void);

        CGlueIO *GetGlueIO(void)
        {
            return mCGlueIO;
        }

    private:
        CGlueIO *mCGlueIO;
    };

    class FileTable
    {
    private:
        /**
         * Helper class to acquire lock and to release it automatically
         * when surrounding block is left.
         */
        class SpinLockHolder
        {
        public:
            SpinLockHolder (CSpinLock &lock) : lockRef(lock)
            {
                lockRef.Acquire ();
            }

            ~SpinLockHolder ()
            {
                lockRef.Release ();
            }

        private:
            CSpinLock &lockRef;
        };

    public:
        static int FindFreeFileSlot (CircleFile *&pFile, unsigned int start_index = 0);
        static int FindFreeDirSlot (_CIRCLE_DIR *&pDir);

        static CircleFile *GetFile(int slot);
        static _CIRCLE_DIR *GetDir(int slot);

        static int DupFd (CircleFile &original_file, int start_slot);

        class FileTableLock : SpinLockHolder
        {
        public:
            FileTableLock() : SpinLockHolder(fileTabLock) {}
        };

        class DirTableLock : SpinLockHolder
        {
        public:
            DirTableLock() : SpinLockHolder(dirTabLock) {}
        };

        static constexpr unsigned int MAX_OPEN_FILES = 20;
        static constexpr unsigned int MAX_OPEN_DIRS = 20;

    private:
        static CircleFile fileTab[MAX_OPEN_FILES];
        static CSpinLock fileTabLock;

        static _CIRCLE_DIR dirTab[MAX_OPEN_DIRS];
        static CSpinLock dirTabLock;
    };
}

#endif

