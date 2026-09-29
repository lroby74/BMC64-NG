#include "config.h"

#include "wrap_fatfs.h"
#include "circle_macros.h"
#include <_ansi.h>
#include <_syslist.h>
#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <circle/input/console.h>
#include <circle/sched/scheduler.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/string.h>
#include <circle/net/socket.h>
#include "circle_glue.h"
#include "cglueio.h"
#include "filetable.h"
#include <assert.h>
#include <stdarg.h>

namespace _CircleStdlib
{
    class CGlueConsole : public CGlueIO
    {
    public:
        enum TConsoleMode
        {
            ConsoleModeRead, ConsoleModeWrite
        };

        CGlueConsole (CConsole &rConsole, TConsoleMode mode)
            :
            mConsole (rConsole), mMode (mode)
        {
        }

        int
        Read (void *pBuffer, int nCount)
        {
            int nResult = -1;

            if (mMode == ConsoleModeRead)
            {
                CScheduler * const scheduler =
                    CScheduler::IsActive () ? CScheduler::Get () : nullptr;
                CUSBController * const usbhost =
                    CUSBHCIDevice::IsActive () ?
                        CUSBHCIDevice::Get () : nullptr;

                while ((nResult = mConsole.Read (pBuffer,
                                                 static_cast<size_t> (nCount)))
                    == 0)
                {
                    if (scheduler)
                    {
                        scheduler->Yield ();
                    }

                    if (usbhost && usbhost->UpdatePlugAndPlay ())
                    {
                        mConsole.UpdatePlugAndPlay ();
                    }
                }
            }

            if (nResult < 0)
            {
                errno = EIO;

                // Could be negative but different from -1.
                nResult = -1;
            }

            return nResult;
        }

        int
        Write (const void *pBuffer, int nCount)
        {
            int nResult =
                mMode == ConsoleModeWrite ?
                    mConsole.Write (pBuffer, static_cast<size_t> (nCount)) : -1;

            if (nResult < 0)
            {
                errno = EIO;

                // Could be negative but different from -1.
                nResult = -1;
            }

            return nResult;
        }

        int
        LSeek(int ptr, int dir)
        {
            errno = ESPIPE;
            return -1;
        }

        int
        Close (void)
        {
            // TODO: Cannot close console file handle currently.
            errno = EBADF;
            return -1;
        }

        int
        FStat (struct stat *buf)
        {
            assert(buf);
            memset(buf, 0, sizeof(*buf));

            // Just some arbitrary but fixed values.
            buf->st_dev = _CircleStdlib::CGlueIO::DeviceIdConsole;
            buf->st_ino = 2000;
            buf->st_nlink = 1;

            // The important flag is S_IFCHR. This is needed by newlib
            // internally to recognize that this is a TTY.
            buf->st_mode = S_IRUSR | S_IWUSR | S_IFCHR;

            return 0;
        }

        int
        IsATty (void)
        {
            return 1;
        }

        TStatus GetSelectStatus (void) const
        {
            // TODO is that true for the console?
            // Reading and writung never blocks.
            return { true, true, true, true };
        }

    private:
        CConsole &mConsole;
        TConsoleMode const mMode;
    };

    /**
     * FatFs: http://www.elm-chan.org/fsw/ff/00index_e.html
     */
    struct CGlueIoFatFs : public CGlueIO
    {
        CGlueIoFatFs ()
            : mFilename (nullptr)
        {
            memset (&mFile, 0, sizeof(mFile));
        }

        ~CGlueIoFatFs ()
        {
            free (mFilename);
        }

        bool
        Open (char *file, int flags, int /* mode */)
        {
            // Remember file name for simulation of fstat ()
            mFilename = (char *) malloc (strlen (file) + 1);
            if (!mFilename)
            {
                errno = ENOMEM;
                return false;
            }
            strcpy(mFilename, file);

            int fatfs_flags;

            /*
             * The OpenGroup documentation of the flags for open() says:
             *
             * "Applications shall specify exactly one of the first
             * three values (file access modes) below in the value of oflag:
             *
             * O_RDONLY Open for reading only.
             * O_WRONLY Open for writing only.
             * O_RDWR Open for reading and writing."
             *
             * So combinations of those flags need not to be dealt with.
             */
            if (flags & O_RDWR)
            {
                fatfs_flags = FA_READ | FA_WRITE;
            }
            else if (flags & O_WRONLY)
            {
                fatfs_flags = FA_WRITE;
            }
            else
            {
                fatfs_flags = FA_READ;
            }

            if (flags & O_TRUNC)
            {
                /*
                 * OpenGroup documentation:
                 * "The result of using O_TRUNC with O_RDONLY is undefined."
                 */
                fatfs_flags |= FA_CREATE_ALWAYS;
            }

            if (flags & O_APPEND)
            {
                fatfs_flags |= FA_OPEN_APPEND;
            }

            if (flags & O_CREAT)
            {
                if (flags & O_EXCL)
                {
                    /*
                     * OpenGroup documentation:
                     * "O_EXCL If O_CREAT and O_EXCL are set, open() shall fail if the file exists.
                     * "If O_EXCL is set and O_CREAT is not set, the result is undefined.".
                     */
                    fatfs_flags |= FA_CREATE_NEW;
                }
                else
                {
                    fatfs_flags |= FA_OPEN_ALWAYS;
                }
            }

            // TODO mode?
            auto const fresult = f_open (&mFile, file, fatfs_flags);

            bool result;
            if (fresult == FR_OK)
            {
                result = true;
            }
            else
            {
                result = false;

                switch (fresult)
                {
                    case FR_EXIST:
                        errno = EEXIST;
                        break;

                    case FR_NO_FILE:
                    case FR_INVALID_NAME:
                        errno = ENOENT;
                        break;

                    case FR_NO_PATH:
                    case FR_INVALID_DRIVE:
                        errno = ENOTDIR;
                        break;

                    case FR_NOT_ENOUGH_CORE:
                        errno = ENOMEM;
                        break;

                    case FR_TOO_MANY_OPEN_FILES:
                        errno = ENFILE;
                        break;

                    case FR_DENIED:
                    case FR_DISK_ERR:
                    case FR_INT_ERR:
                    case FR_NOT_READY:
                    case FR_INVALID_OBJECT:
                    case FR_WRITE_PROTECTED:
                    case FR_NOT_ENABLED:
                    case FR_NO_FILESYSTEM:
                    case FR_TIMEOUT:
                    case FR_LOCKED:
                    default:
                        errno = EACCES;
                        break;
                }
            }

            return result;
        }

        int
        Read (void *pBuffer, int nCount)
        {
            UINT bytes_read = 0;
            auto const fresult = f_read (&mFile, pBuffer,
                                         static_cast<UINT> (nCount),
                                         &bytes_read);

            int result;
            if (fresult == FR_OK)
            {
                result = static_cast<int> (bytes_read);
            }
            else
            {
                result = -1;

                switch (fresult)
                {
                    case FR_INVALID_OBJECT:
                    case FR_DENIED:
                        errno = EBADF;
                        break;

                    case FR_DISK_ERR:
                    case FR_INT_ERR:
                    case FR_TIMEOUT:
                    default:
                        errno = EIO;
                        break;
                }
            }

            return result;
        }

        int
        Write (const void *pBuffer, int nCount)
        {
            UINT bytesWritten = 0;
            auto const fresult = f_write (&mFile, pBuffer,
                                          static_cast<UINT> (nCount),
                                          &bytesWritten);

            int result;
            if (fresult == FR_OK)
            {
                result = static_cast<int> (bytesWritten);
            }
            else
            {
                result = -1;

                switch (fresult)
                {
                    case FR_INVALID_OBJECT:
                        errno = EBADF;
                        break;

                    case FR_DISK_ERR:
                    case FR_INT_ERR:
                    case FR_DENIED:
                    case FR_TIMEOUT:
                    default:
                        errno = EIO;
                        break;
                }
            }

            return result;
        }

        int
        LSeek(int ptr, int dir)
        {
            /*
             * If whence is SEEK_SET, the file offset shall be set to
             * offset bytes.
             *
             * If whence is SEEK_CUR, the file offset shall be set to
             * its current location plus offset.
             *
             * If whence is SEEK_END, the file offset shall be set to
             * the size of the file plus offset.
             */
            int new_pos;
            switch (dir)
            {
                case SEEK_SET:
                    new_pos = ptr;
                    break;

                case SEEK_CUR:
                    new_pos = static_cast<int>(f_tell(&mFile)) + ptr;
                    break;

                case SEEK_END:
                    new_pos = static_cast<int>(f_size(&mFile)) + ptr;
                    break;

                default:
                    new_pos = -1;
                    break;
            }

            int result;
            if (new_pos >= 0)
            {
                int const fresult = f_lseek(&mFile, static_cast<FSIZE_t>(new_pos));

                switch (fresult)
                {
                    case FR_OK:
                        result = new_pos;
                        break;

                    case FR_DISK_ERR:
                    case FR_INT_ERR:
                    case FR_INVALID_OBJECT:
                    case FR_TIMEOUT:
                    default:
                        result = -1;
                        errno = EBADF;
                        break;
                }
            }
            else
            {
                errno = EINVAL;
                result = -1;
            }

            return result;
        }

        int
        Close (void)
        {
            auto const close_result = f_close (&mFile);

            int result;
            switch (close_result)
            {
                case FR_OK:
                    result = 0;
                    break;

                case FR_INVALID_OBJECT:
                case FR_INT_ERR:
                    errno = EBADF;
                    result = -1;
                    break;

                default:
                    errno = EIO;
                    result = -1;
                    break;
            }

            if (mFilename)
            {
                free (mFilename);
                mFilename = nullptr;
            }

            return result;
        }

        virtual int
        FTruncate (off_t length)
        {
            if (length < 0)
            {
                errno = EINVAL;
                return -1;
            }

            FSIZE_t const cur_pos = f_tell(&mFile);

            if (f_lseek(&mFile, (FSIZE_t) length) != F_OK)
            {
                errno = EIO;
                return -1;
            }

            FRESULT const fresult = f_truncate(&mFile);
            int result;
            if (fresult == F_OK)
            {
                result = 0;
            }
            else
            {
                result = -1;

                // Best effort to map error codes:
                switch (fresult)
                {
                case FR_DENIED:
                    errno = EINVAL;
                    break;

                default:
                    errno = EIO;
                    break;
                }
            }

            // Best effort to restore seek position even if error occurred.
            f_lseek(&mFile, cur_pos);

            return result;
        }

        int
        FSync (void)
        {
            int const result = f_sync (&mFile) == FR_OK ? 0 : -1;

            if (result != 0)
            {
                errno = EIO;
            }

            return result;
        }

        int
        FStat (struct stat *buf)
        {
            // We need to force an fsync() because the fstat is simulated
            // via a stat() call using the saved filename.
            if (FSync () == -1)
            {
                return -1;
            }

            assert (mFilename);
            return stat (mFilename, buf);
        }

        int
        IsATty (void)
        {
            errno = ENOTTY;
            return 0;
        }

        TStatus GetSelectStatus (void) const
        {
            return { true, true, true, true };
        }

    public:
        FIL mFile;
        char *mFilename;
    };

#include "io_slurp.h"

    void
    CGlueInitConsole (CConsole &rConsole)
    {
        _CircleStdlib::CircleFile * const stdin = _CircleStdlib::FileTable::GetFile(0);
        _CircleStdlib::CircleFile * const stdout = _CircleStdlib::FileTable::GetFile(1);
        _CircleStdlib::CircleFile * const stderr = _CircleStdlib::FileTable::GetFile(2);

        // Must only be called once and not be called after a file has already been opened
        assert(!stdin->IsOpen());
        assert(!stdout->IsOpen());
        assert(!stderr->IsOpen());

        stdin->AssignGlueIO(*new CGlueConsole (rConsole,
                                           CGlueConsole::ConsoleModeRead));
        stdout->AssignGlueIO(*new CGlueConsole (rConsole,
                                            CGlueConsole::ConsoleModeWrite));
        stderr->AssignGlueIO(*new CGlueConsole (rConsole,
                                            CGlueConsole::ConsoleModeWrite));
    }
}

void
CGlueStdioInit (CConsole &rConsole)
{
     _CircleStdlib::CGlueInitConsole (rConsole);
}

extern "C" int
_open (char *file, int flags, int mode)
{
    _CircleStdlib::FileTable::FileTableLock fileTabLock;

    _CircleStdlib::CircleFile *circleFile;
    int slot = _CircleStdlib::FileTable::FindFreeFileSlot (circleFile);

    if (slot != -1)
    {
        assert(circleFile != nullptr);

        auto const newFatFs = new _CircleStdlib::CGlueIoSlurpFatFs ();

        if (newFatFs->Open (file, flags, mode))
        {
            circleFile->AssignGlueIO(*newFatFs);
        }
        else
        {
            delete newFatFs;
            slot = -1;
        }
    }
    else
    {
        errno = ENFILE;
    }

    return slot;
}

extern "C" int
_close (int fildes)
{
    _CircleStdlib::FileTable::FileTableLock fileTabLock;

    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);
    if (!file)
    {
        errno = EBADF;
        return -1;
    }

    if (!file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    _CircleStdlib::CGlueIO * const glueIO = file->GetGlueIO();
    assert (glueIO);
    assert (glueIO->GetRefCount () > 0);
    glueIO->DecrementRefCount ();

    int result;
    if (glueIO->GetRefCount () == 0)
    {
        result = glueIO->Close ();
        file->CloseGlueIO ();
    }
    else
    {
        result = 0;
    }

    if (CScheduler::IsActive ())
    {
        CScheduler::Get ()->Yield ();
    }

    return result;
}

extern "C" int
_read (int fildes, char *ptr, int len)
{
    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);
    if (!file)
    {
        errno = EBADF;
        return -1;
    }

    if (!file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    return file->GetGlueIO()->Read (ptr, len);
}

extern "C" int
_write (int fildes, char *ptr, int len)
{
    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);
    if (!file)    {
        errno = EBADF;
        return -1;
    }

    if (!file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    return file->GetGlueIO ()->Write (ptr, len);
}

extern "C" int
_lseek(int fildes, int ptr, int dir)
{
    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);

    if (!file || !file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    return file->GetGlueIO()->LSeek (ptr, dir);
}

extern "C" int
_fstat (int fildes, struct stat *st)
{
    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);

    if (!file || !file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    return file->GetGlueIO()->FStat (st);
}

extern "C" int
ftruncate (int fildes, off_t length)
{
    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);

    if (!file || !file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    return file->GetGlueIO()->FTruncate (length);
}

extern "C" int
truncate (char const *path, off_t length)
{
    if (length < 0)
    {
        errno = EINVAL;
        return -1;
    }

    // Use stat() to verify the file exists and is a regular file
    struct stat st;
    if (stat (path, &st) != 0)
    {
        return -1;  // errno set by stat()
    }

    if (S_ISDIR (st.st_mode))
    {
        errno = EISDIR;
        return -1;
    }

    FIL file;
    memset (&file, 0, sizeof (file));
    FRESULT const open_result = f_open (&file, path, FA_READ | FA_WRITE);
    if (open_result != FR_OK)
    {
        switch (open_result)
        {
            case FR_NO_FILE:
            case FR_INVALID_NAME:
                errno = ENOENT;
                break;

            case FR_NO_PATH:
            case FR_INVALID_DRIVE:
                errno = ENOTDIR;
                break;

            case FR_DENIED:
            case FR_WRITE_PROTECTED:
                errno = EACCES;
                break;

            default:
                errno = EIO;
                break;
        }
        return -1;
    }

    int result = 0;
    FSIZE_t const current_size = f_size (&file);

    if (static_cast<FSIZE_t> (length) < current_size)
    {
        // Shorten: seek to desired length, then truncate
        if (f_lseek (&file, static_cast<FSIZE_t> (length)) != FR_OK)
        {
            errno = EIO;
            result = -1;
        }
        else
        {
            FRESULT const fresult = f_truncate (&file);
            if (fresult != FR_OK)
            {
                switch (fresult)
                {
                    case FR_DENIED:
                        errno = EACCES;
                        break;

                    default:
                        errno = EIO;
                        break;
                }
                result = -1;
            }
        }
    }
    else if (static_cast<FSIZE_t> (length) > current_size)
    {
        // Extend: seek to current end, zero-fill to new length
        if (f_lseek (&file, current_size) != FR_OK)
        {
            errno = EIO;
            result = -1;
        }
        else
        {
            char const zero_buf[512] = {};
            FSIZE_t remaining = static_cast<FSIZE_t> (length) - current_size;

            while (remaining > 0 && result == 0)
            {
                UINT const chunk = static_cast<UINT> (
                    remaining < sizeof (zero_buf) ? remaining : sizeof (zero_buf));
                UINT bytes_written = 0;
                FRESULT const wresult = f_write (&file, zero_buf, chunk, &bytes_written);
                if (wresult != FR_OK || bytes_written != chunk)
                {
                    errno = EIO;
                    result = -1;
                }
                else
                {
                    remaining -= bytes_written;
                }
            }
        }
    }
    // else length == current_size: nothing to do

    f_close (&file);
    return result;
}

template<int (_CircleStdlib::CGlueIO::*func) (void)>
int call_glueio_func_void_arg_valid_fildes(int fildes)
{
    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);

    if (!file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    return (file->GetGlueIO()->*func) ();
}

template<int (_CircleStdlib::CGlueIO::*func) (void)>
int call_glueio_func_void_arg(int fildes)
{
    _CircleStdlib::CircleFile * const file = _CircleStdlib::FileTable::GetFile(fildes);

    if (!file)
    {
        errno = EBADF;
        return -1;
    }

    return call_glueio_func_void_arg_valid_fildes<func> (fildes);
}

extern "C" int
fsync (int fildes)
{
    return call_glueio_func_void_arg<&_CircleStdlib::CGlueIO::FSync> (fildes);
}

extern "C" int
_isatty (int fildes)
{
    return call_glueio_func_void_arg<&_CircleStdlib::CGlueIO::IsATty> (fildes);
}

extern "C" DIR *
opendir (const char *name)
{
    _CircleStdlib::FileTable::DirTableLock dirTableLock;

    _CIRCLE_DIR *circleDir;
    int const slotNum = _CircleStdlib::FileTable::FindFreeDirSlot (circleDir);
    if (slotNum == -1)
    {
        errno = ENFILE;
        return nullptr;
    }

    assert(circleDir);

    FRESULT const fresult = f_opendir (&circleDir->mCurrentEntry, name);

    /*
     * Best-effort mapping of FatFs error codes to errno values.
     */
    DIR *result = nullptr;
    switch (fresult)
    {
        case FR_OK:
            circleDir->mOpen = 1;
            circleDir->mFirstRead = 1;
            result = circleDir;
            break;

        case FR_DISK_ERR:
        case FR_INT_ERR:
        case FR_NOT_READY:
        case FR_INVALID_OBJECT:
            errno = EACCES;
            break;

        case FR_NO_PATH:
        case FR_INVALID_NAME:
        case FR_INVALID_DRIVE:
        case FR_NOT_ENABLED:
        case FR_NO_FILESYSTEM:
        case FR_TIMEOUT:
            errno = ENOENT;
            break;

        case FR_NOT_ENOUGH_CORE:
        case FR_TOO_MANY_OPEN_FILES:
            errno = ENFILE;
            break;
        
        default:
            // Should never happen, the default: avoids a warning with LLVM
            errno = EIO;
            break;
    }

    return result;
}

static bool
read_next_entry (FATFS_DIR *currentEntry, FILINFO *filinfo)
{
    bool result;

    if (f_readdir (currentEntry, filinfo) == FR_OK)
    {
        result = filinfo->fname[0] != 0;
    }
    else
    {
        errno = EBADF;
        result = false;
    }

    return result;
}

static struct dirent*
do_readdir (DIR *dir, struct dirent *de)
{
    FILINFO filinfo;
    bool haveEntry;

    if (dir->mFirstRead)
    {
        if (f_readdir (&dir->mCurrentEntry, nullptr) == FR_OK)
        {
            haveEntry = read_next_entry (&dir->mCurrentEntry, &filinfo);
        }
        else
        {
            errno = EBADF;
            haveEntry = false;
        }
        dir->mFirstRead = 0;
    }
    else
    {
        haveEntry = read_next_entry (&dir->mCurrentEntry, &filinfo);
    }

    struct dirent *result;
    if (haveEntry)
    {
        memcpy (de->d_name, filinfo.fname, sizeof(de->d_name));
        de->d_ino = 0; // inode number does not exist in fatfs
        result = de;
    }
    else
    {
        // end of directory does not change errno
        result = nullptr;
    }

    return result;
}

extern "C" struct dirent*
readdir (DIR *dir)
{
    struct dirent *result;

    if (dir->mOpen)
    {
        result = do_readdir (dir, &dir->mEntry);
    }
    else
    {
        errno = EBADF;
        result = nullptr;
    }

    return result;
}

extern "C" int
readdir_r (DIR *__restrict dir, dirent *__restrict de, dirent **__restrict ode)
{
    int result;

    if (dir->mOpen)
    {
        *ode = do_readdir (dir, de);
        result = 0;
    }
    else
    {
        *ode = nullptr;
        result = EBADF;
    }

    return result;
}

extern "C" void
rewinddir (DIR *dir)
{
    dir->mFirstRead = 1;
}

extern "C" int
closedir (DIR *dir)
{
    _CircleStdlib::FileTable::DirTableLock dirTableLock;

    int result;

    if (dir->mOpen)
    {
        dir->mOpen = 0;

        if (f_closedir (&dir->mCurrentEntry) == FR_OK)
        {
            result = 0;
        }
        else
        {
            errno = EBADF;
            result = -1;
        }
    }
    else
    {
        errno = EBADF;
        result = -1;
    }

    return result;
}

extern "C" int
_fcntl (int fildes, int cmd, ...)
{
    _CircleStdlib::FileTable::FileTableLock fileTabLock;

    _CircleStdlib::CircleFile * const original_file = _CircleStdlib::FileTable::GetFile(fildes);

    if (!original_file || !original_file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    va_list args;
    va_start(args, cmd);
    int const arg = va_arg(args, int);
    va_end(args);

    if (cmd == F_DUPFD)
    {
        return _CircleStdlib::FileTable::DupFd (*original_file, arg);
    }
    else
    {
        return original_file->GetGlueIO()->Fcntl (cmd, arg);
    }
}

extern "C" int
dup(int fildes)
{
    return fcntl (fildes, F_DUPFD, 0);
}

extern "C" int
dup2 (int fildes, int fildes2)
{
    // From the OpenGroup specification:
    // "If fildes2 is less than 0 or greater than or equal to {OPEN_MAX},
    // dup2() shall return -1 with errno set to [EBADF]."
    if (fildes2 < 0 || static_cast<unsigned int> (fildes2) >= _CircleStdlib::FileTable::MAX_OPEN_FILES)
    {
        errno = EBADF;
        return -1;
    }

    _CircleStdlib::FileTable::FileTableLock fileTabLock;

    _CircleStdlib::CircleFile * const original_file = _CircleStdlib::FileTable::GetFile(fildes);

    if (!original_file || !original_file->IsOpen())
    {
        errno = EBADF;
        return -1;
    }

    // From the OpenGroup specification:
    // "If fildes is a valid file descriptor and is equal to fildes2,
    // dup2() shall return fildes2 without closing it."
    if (fildes == fildes2)
    {
        return fildes2;
    }

    _close (fildes2);
    int const result = _CircleStdlib::FileTable::DupFd  (*original_file, fildes2);

    assert (result == -1 || result == fildes2);

    return result;
}
