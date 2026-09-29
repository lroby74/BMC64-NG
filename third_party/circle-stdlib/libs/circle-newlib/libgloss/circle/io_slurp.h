#ifndef _CIRCNEWLIB_IO_SLURP_H
#define _CIRCNEWLIB_IO_SLURP_H

#include "cglueio.h"
#include "wrap_fatfs.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>


    struct CGlueIoSlurpFatFs : public CGlueIoFatFs
    {
        char *mRAMBuffer;
        int mAllocated;
        unsigned mSize;
        unsigned mPosition;
        int mMode;
        int mWrittenTo;

        CGlueIoSlurpFatFs()
            : mRAMBuffer(nullptr), mAllocated(0), mSize(0), mPosition(0), mMode(0), mWrittenTo(0)
        {
        }

        ~CGlueIoSlurpFatFs()
        {
            if (mRAMBuffer) {
                free(mRAMBuffer);
                mRAMBuffer = nullptr;
            }
        }

        int slurp_file() {
            if (mRAMBuffer == nullptr) {
                mSize = 0;
                unsigned total = 0;
                if (f_lseek(&mFile, 0) != FR_OK) {
                   return -1;
                }
                while (true) {
                  char readBuf[1024];
                  unsigned int num_read;
                  if (f_read(&mFile, readBuf, 1024, &num_read) != FR_OK) {
                    if (mRAMBuffer) {
                       free(mRAMBuffer);
                       mRAMBuffer = nullptr;
                    }  
                    return -1;
                  }
                  if (num_read == 0) {
                    break;
                  }
                  if (mRAMBuffer == nullptr) {
                    mAllocated = 1024;
                    mRAMBuffer = (char *)malloc(mAllocated);
                  } else if ((unsigned int)mAllocated < total + num_read) {
                    mAllocated *= 2;
                    mRAMBuffer = (char *)realloc(mRAMBuffer, mAllocated);
                  }
                  memcpy(mRAMBuffer + total, readBuf, num_read);
                  total += num_read;
                  mSize = total;
                }
            }
            return 0;
        }

        bool Open(char *file, int flags, int mode)
        {
            mMode = flags & 7;


            bool r = CGlueIoFatFs::Open(file, flags, mode);
            if (!r) return false;


            if (mMode == O_RDWR || mMode == O_RDONLY) {
               if (slurp_file()) {
                  errno = ENFILE;
                  return false;
               }
            }
            return true;
        }

        int Read(void *pBuffer, int nCount)
        {
            if (mRAMBuffer == nullptr) {
                return CGlueIoFatFs::Read(pBuffer, nCount);
            } else {
                 unsigned int max = nCount;
                 unsigned int remain = mSize - mPosition;
                 if (max > remain) max = remain;
                 if (max > 0) {
                    memcpy(pBuffer, mRAMBuffer + mPosition, max);
                    mPosition += max;
                 }
                 return static_cast<int>(max);
            }
        }

        int Write(const void *pBuffer, int nCount)
        {
            mWrittenTo = 1;

            if (mRAMBuffer == nullptr) {
                mAllocated = 1024;
                mRAMBuffer = (char *) malloc(mAllocated);
            }


            while (mPosition + nCount >= (unsigned int)mAllocated) {
                mAllocated *= 2;
                mRAMBuffer = (char *)realloc(mRAMBuffer, mAllocated);
            }



            if (mPosition > mSize) {
                memset(mRAMBuffer + mSize, 0, mPosition - mSize);
            }

            memcpy(mRAMBuffer + mPosition, pBuffer, nCount);
            mPosition += nCount;
            if (mPosition > mSize) {
                mSize = mPosition;
            }


            if (mMode == O_WRONLY) {
                UINT bytesWritten = 0;
                f_write(&mFile, pBuffer, static_cast<UINT>(nCount), &bytesWritten);
            }
            
            return nCount;
        }

        int LSeek(int ptr, int dir)
        {
            if (mRAMBuffer == nullptr) {
                return CGlueIoFatFs::LSeek(ptr, dir);
            }

            int new_pos;
            switch (dir) {
                case SEEK_SET: new_pos = ptr; break;
                case SEEK_CUR: new_pos = mPosition + ptr; break;
                case SEEK_END: new_pos = mSize + ptr; break;
                default: errno = EINVAL; return -1;
            }







            if (new_pos < 0) {
                errno = EINVAL;
                return -1;
            }

            mPosition = new_pos;
            return mPosition;
        }

        int Close(void)
        {
            if (mRAMBuffer) {


                if (mMode == O_RDWR && mWrittenTo) {
                    f_close(&mFile);
                    if (f_open(&mFile, mFilename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
                        UINT bytesWritten = 0;
                        f_write(&mFile, mRAMBuffer, mSize, &bytesWritten);
                    }
                }
            }
            
            return CGlueIoFatFs::Close();
        }
    };
#endif
