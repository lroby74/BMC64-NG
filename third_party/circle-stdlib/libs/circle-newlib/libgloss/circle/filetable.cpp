#include <errno.h>
#include <assert.h>

#include "circle_macros.h"
#include "circle_glue.h"
#include "cglueio.h"
#include "filetable.h"

namespace _CircleStdlib
{
    CircleFile FileTable::fileTab[];
    CSpinLock FileTable::fileTabLock { TASK_LEVEL };

    _CIRCLE_DIR FileTable::dirTab[];
    CSpinLock FileTable::dirTabLock { TASK_LEVEL };

    void CircleFile::CloseGlueIO(void)
    {
        assert(mCGlueIO);

        delete mCGlueIO;
        mCGlueIO = nullptr;
    }

    int
    FileTable::FindFreeFileSlot(CircleFile *&pFile, unsigned int start_index)
    {
        int slotNr = -1;

        for (unsigned int i = start_index; i < MAX_OPEN_FILES; i += 1)
        {
            auto &slot = fileTab[i];

            if (!slot.IsOpen())
            {
                pFile = &slot;
                slotNr = &slot - fileTab;
                break;
            }
        }

        return slotNr;
    }

    CircleFile *FileTable::GetFile(int slot)
    {
        return slot >= 0 && slot < MAX_OPEN_FILES ? fileTab + slot : nullptr;
    }

    int
    FileTable::FindFreeDirSlot(_CIRCLE_DIR *&pDir)
    {
        int slotNr = -1;

        for (auto &slot : dirTab)
        {
            if (!slot.mOpen)
            {
                pDir = &slot;
                slotNr = &slot - dirTab;
                break;
            }
        }

        return slotNr;
    }

    _CIRCLE_DIR *FileTable::GetDir(int slot)
    {
        return slot >= 0 && slot < MAX_OPEN_DIRS ? dirTab + slot : nullptr;
    }

    int FileTable::DupFd (CircleFile &original_file, int start_slot)
    {
        if (start_slot < 0 || start_slot >= MAX_OPEN_FILES)
        {
            errno = EINVAL;
            return -1;
        }

        CircleFile *new_file = nullptr;
        int const result =
            FindFreeFileSlot(new_file, static_cast<unsigned int>(start_slot));

        if (result != -1)
        {
            assert(new_file);
            CGlueIO * const glue_io = original_file.GetGlueIO();
            assert(glue_io);
            new_file->AssignGlueIO(*glue_io);
            glue_io->IncrementRefCount();
        }
        else
        {
            errno = EMFILE;
        }

        return result;
    }
}

