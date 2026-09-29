#include "config.h"
#include <_ansi.h>
#include <_syslist.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include "third_party/circle-stdlib/include/wrap_fatfs.h"
#include <sys/dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#undef errno
extern int errno;
#include "third_party/circle-stdlib/libs/circle-newlib/libgloss/circle/warning.h"

#include "circle_glue.h"
#include <assert.h>

#include <malloc.h>
#include <stdio.h>
#include <sys/unistd.h>
#include <circle/serial.h>
#include <circle/synchronize.h>
#include <circle/timer.h>
#include <circle/multicore.h>
#include <circle/spinlock.h>
#include "third_party/common/circle.h"





struct SchedaInUso {



  SchedaInUso(const char *cosa) {







    if (bmc_vice_partito && CMultiCoreSupport::ThisCore() == 0) {
      static volatile int avviso_in_corso = 0;
      bmc_core0_nella_scheda++;
      bmc_core0_nella_scheda_cosa = cosa;
      if (!avviso_in_corso) {
        avviso_in_corso = 1;
        printf("[CORE0-SCHEDA] %s\n", cosa);
        avviso_in_corso = 0;
      }
    }
    bmc_scheda_in_uso++;
    DataMemBarrier();
    while (bmc_scheda_core0) {
      bmc_scheda_in_uso--;
      DataMemBarrier();
      while (bmc_scheda_core0) {
      }
      bmc_scheda_in_uso++;
      DataMemBarrier();
    }
    if (bmc_scheda_in_uso == 1) {

      bmc_scheda_da = CTimer::GetClockTicks();
    }
    bmc_scheda_cosa = cosa;
    bmc_scheda_file = nullptr;
  }
  ~SchedaInUso() {
    DataMemBarrier();
    bmc_scheda_in_uso--;
  }
};

struct _CIRCLE_DIR {
  _CIRCLE_DIR() : mFirstRead(0), mOpen(0) {
    mEntry.d_ino = 0;
    mEntry.d_type = DT_UNKNOWN;
    mEntry.d_name[0] = 0;
  }

  FATFS_DIR mCurrentEntry;
  struct dirent mEntry;
  unsigned int mFirstRead : 1;
  unsigned int mOpen : 1;
};

// This is a replacement io.cpp specifically for BMC64.
// This implementation will sometimes load the entire file
// into memory to provide faster seek operations, improving
// performance on slow SD cards.  Since any file the emulator
// attempts to load is relatively small (<200k), this works out
// just fine for our needs. Obviously, this would not be a
// viable solution for most other circumstances.  It also
// works around an issue with circle/fatfs integration that
// was causing memory corruption.
//
// When a file is opened for READ ONLY, fatfs is used to open
// the file.  As long as the client never seeks, the file will
// not be loaded into ram and the disk still backs the data.  As
// soon as seek is called, the file will be loaded into ram and
// from then on, ram backs the data.  NOTE the fatfs file remains
// open even after the file is loaded into ram in this case. If
// the client never calls seek, the data will be read from fatfs.
//
// When a file is opened for WRITE ONLY, fatfs is used to create
// the file. However, all write operations write to ram and only
// when the file is finally closed will the data be dumped to
// the fat fs filesystem.  The fatfs file remains open during
// the entire time between open/close.  Seek is technically
// supported in this case but attempting to seek past the
// current file size is not.  Call to fstat on a file in WRTE_ONLY
// mode will not work as expected.
//
// When a file is opened for READ_WRITE, fat fs is used to
// immediately load the contents of the existing file into RAM,
// while retaining the read/write FatFs handle. Reads and seeks use
// the RAM copy; writes also update the corresponding range in the
// backing file. The caller's fsync() requests commit those writes to
// the storage device. Seeking past the current file length is not
// supported.






#define MAX_OPEN_FILES 32
#define MAX_OPEN_DIRS 10



#define SLURP_CHUNK 32768



#define WRITE_BUF_SIZE 1024
















#ifndef SLURP_MAX_BYTES
#define SLURP_MAX_BYTES (32u * 1024u * 1024u)
#endif




#define STREAM_MAX_BYTES 0x7F000000u

static const char *pattern = "*";









#define CIRCLE_PATH_MAX 1024

static char currentDir[CIRCLE_PATH_MAX];

/**
 * @fn int strend(const char *s, const char *t)
 * @brief Searches the end of string s for string t
 * @param s the string to be searched
 * @param t the substring to locate at the end of string s
 * @return one if the string t occurs at the end of the string s, and zero otherwise
 */
int strend(const char *s, const char *t)
{
    size_t ls = strlen(s); // find length of s
    size_t lt = strlen(t); // find length of t
    if (ls >= lt)  // check if t can fit in s
    {
        // point s to where t should start and compare the strings from there
        return (0 == memcmp(t, s + (ls - lt), lt));
    }
    return 0; // t was longer than s
}

static void reverse(char *x, int begin, int end) {
  char c;

  if (begin >= end)
    return;

  c = *(x + begin);
  *(x + begin) = *(x + end);
  *(x + end) = c;

  reverse(x, ++begin, --end);
}

static void itoa2(int i, char *dst) {
  int q = 0;
  int j;
  do {
    j = i % 10;
    dst[q] = '0' + j;
    q++;
    i = i / 10;
  } while (i > 0);
  dst[q] = '\0';

  reverse(dst, 0, strlen(dst) - 1);
}

CSerialDevice *g_serial;

static void logm(const char *msg) {
   if (g_serial) {
      g_serial->Write(msg, strlen(msg));
   }
}

static void logi(int i) {
   char nn[16];
   itoa2(i,nn);
   if (g_serial) {
      g_serial->Write(nn, strlen(nn));
   }
}

struct CirclePath {
   CirclePath(const char* p) {
      path[0] = '\0';
      troppo_lungo = 0;

      if (p == nullptr) {
         return;
      }

      int len = strlen(p);
      if (len == 0) {
         return;
      }

      if (p[0] == '/') {
         // Absolute
         copia(p, "");
         return;
      }

      // Relative
      copia(currentDir, "");
      if (len == 1 && p[0] == '.') {
         // Treat as current dir
         return;
      }

      // Handle ./ at start but we don't in the middle.
      if (len >= 2 && p[0] == '.' && p[1] == '/') {
         copia(currentDir, p+2);
      } else {
         copia(p, "");
      }

      // Fat fs doesn't like trailing slashes for dirs
      size_t n = strlen(path);
      if (n > 1 && path[n-1] == '/') {
         path[n-1] = '\0';
      }
   }




   void copia(const char *a, const char *b) {
      size_t la = strlen(a);
      size_t lb = strlen(b);
      if (la + lb >= sizeof(path)) {
         path[0] = '\0';
         troppo_lungo = 1;
         return;
      }
      memcpy(path, a, la);
      memcpy(path + la, b, lb + 1);
   }

   char path[CIRCLE_PATH_MAX];
   int troppo_lungo;
};

struct CircleFile {
  FIL file;
  int in_use;
  char fname[CIRCLE_PATH_MAX];

  char *contents; // bytes for file in memory
  int allocated; // total bytes allocated for in memory file
  unsigned size; // total size of file in memory file
  unsigned position; // current in memory write position
  int mode; // remembers mode this file was opened under
  int written_to;



  unsigned int riversato_fino_a;
  int fopen_called; // f_open was called and thus f_close needs to be called
  int streamed;
  DWORD *cltbl;




  int in_coda;
  unsigned int coda_da;


  int da_sincronizzare;
};

struct CircleDir {
  CircleDir() {
    in_use = 0;
  }

  DIR dir;
  int in_use;
};

CircleFile fileTab[MAX_OPEN_FILES];
CircleDir dirTab[MAX_OPEN_DIRS];

static const char* const VolumeStr[FF_VOLUMES] = {FF_VOLUME_STRS};
#if FF_MULTI_PARTITION
PARTITION VolToPart[FF_VOLUMES];
#endif

void CGlueStdioInit(CSerialDevice *serial) {
  g_serial = serial;

  if (g_serial) {
    setvbuf(stdout, nullptr, _IOLBF, 0);
    setvbuf(stderr, nullptr, _IOLBF, 0);
  }

  // Initialize stdio, stderr and stdin
  fileTab[0].in_use = 1;
  fileTab[1].in_use = 1;
  fileTab[2].in_use = 1;

  // By default, use the first partition of each physical drive.
#if FF_MULTI_PARTITION
  for (int pd = 0; pd < FF_VOLUMES; pd++) {
    VolToPart[pd].pd = pd;
    VolToPart[pd].pt = 0;
  }
#endif

  strcpy (currentDir, "/");
}

static int g_bootStatNum = 0;
static int *g_bootStatWhat;
static const char **g_bootStatFile;
static int *g_bootStatSize;

// Set global vars pointing to bootstat info
void CGlueStdioInitBootStat (int num,
        int *bootStatWhat,
        const char **bootStatFile,
        int *bootStatSize) {
   g_bootStatNum = num;
   g_bootStatWhat = bootStatWhat;
   g_bootStatFile = bootStatFile;
   g_bootStatSize = bootStatSize;
}

void CGlueStdioSetPartitionForVolume (const char* volume, int part, unsigned int ss) {
#if FF_MULTI_PARTITION
  for (int pd = 0; pd < FF_VOLUMES; pd++) {
     if (strcmp(volume, VolumeStr[pd]) == 0) {
        VolToPart[pd].pt = part;
        return;
     }
  }
#else
  (void) volume;
  (void) part;
  (void) ss;
#endif
}

static int FindFreeFileSlot(void) {
  int slotNr = -1;

  for (const CircleFile &slot : fileTab) {
    if (slot.in_use == 0) {
      slotNr = &slot - fileTab;
      break;
    }
  }

  return slotNr;
}





extern "C" int circle_file_aperti(int stampa) {
  int n = 0;

  for (int i = 3; i < MAX_OPEN_FILES; i++) {
    if (fileTab[i].in_use) {
      n++;
    }
  }
  if (stampa) {
    printf("[FT] aperti %d su %d\n", n, MAX_OPEN_FILES - 3);
    for (int i = 3; i < MAX_OPEN_FILES; i++) {
      if (fileTab[i].in_use) {
        printf("[FT]   %d: %s (modo %d, a flusso %d)\n", i, fileTab[i].fname,
               fileTab[i].mode, fileTab[i].streamed);
      }
    }
  }
  return n;
}

static char *strdup2(const char *s) {
  char *d = (char *)malloc(strlen(s) + 1);
  if (d == nullptr)
    return nullptr;
  strcpy(d, s);
  return d;
}


static int FindFreeDirSlot(void) {
  int slotNr = -1;

  for (const CircleDir &slot : dirTab) {
    if (!slot.in_use) {
      slotNr = &slot - dirTab;
      break;
    }
  }

  return slotNr;
}

static CircleDir *FindCircleDirFromDIR(DIR *dir) {
  for (CircleDir &slot : dirTab) {
    if (slot.in_use && dir == &slot.dir) {
      return &slot;
    }
  }
  return nullptr;
}

// Returns non zero value on any failure. Any memory will be
// freed on error and file.contents nulled.



























static int e_una_reu(const char *percorso) {
  size_t n;

  if (percorso == nullptr) {
    return 0;
  }
  n = strlen(percorso);
  if (n < 4 || percorso[n - 4] != '.') {
    return 0;
  }
  return (percorso[n - 3] | 0x20) == 'r' && (percorso[n - 2] | 0x20) == 'e' &&
         (percorso[n - 1] | 0x20) == 'u';
}

static int e_un_dischetto(const char *percorso) {
  static const char *const dischetti[] = {"FLP1:", "FLP2:", nullptr};

  if (percorso == nullptr) {
    return 0;
  }
  for (int v = 0; dischetti[v] != nullptr; v++) {
    int i = 0;
    while (dischetti[v][i] != '\0') {
      char c = percorso[i];
      if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
      }
      if (c != dischetti[v][i]) {
        break;
      }
      i++;
    }
    if (dischetti[v][i] == '\0') {
      return 1;
    }
  }
  return 0;
}

static int slurp_file(CircleFile &file) {
  if (file.contents == nullptr) {







    unsigned size = (unsigned)f_size(&file.file);
    file.size = 0;
    if (size == 0) {

      return 0;
    }
    file.allocated = 0;

    if (f_lseek(&file.file, 0) != FR_OK) {
       return -1;
    }

    file.contents = (char *)malloc(size);
    if (file.contents == nullptr) {


       return -2;
    }
    file.allocated = (int)size;


    unsigned long t0 = (unsigned long)CTimer::GetClockTicks64();
    unsigned total = 0;
    while (total < size) {
      unsigned int want = size - total;
      if (want > SLURP_CHUNK) {
         want = SLURP_CHUNK;
      }
      unsigned int num_read;
      if (f_read(&file.file, file.contents + total, want, &num_read) != FR_OK) {
        free(file.contents);
        file.contents = nullptr;
        file.allocated = 0;
        return -1;
      }
      if (num_read == 0) {


        break;
      }
      total += num_read;
    }
    file.size = total;
    if (total >= 1024u * 1024u) {
      unsigned long ms = ((unsigned long)CTimer::GetClockTicks64() - t0) / 1000;

      printf("[IO] %s: %u KB in RAM in %lu ms (%lu KB/s)\n", file.fname,
             total / 1024, ms, ms > 0 ? (unsigned long)total / ms : 0);
    }
  }
  return 0;
}






extern "C" void assertion_failed(const char *pExpr, const char *pFile,
                                 unsigned nLine) {
  static int already_here = 0;
  char line[512];
  int n;

  n = snprintf(line, sizeof(line),
               "\r\n*** BMC64 STOPPED: failed assert ***\r\n"
               "    %s\r\n    %s line %u\r\n",
               pExpr ? pExpr : "?", pFile ? pFile : "?", nLine);
  if (n < 0) {
    n = 0;
  }

  if (g_serial != nullptr) {
    g_serial->Write(line, n);
  }




  if (!already_here && n > 0) {
    FIL f;
    already_here = 1;
    if (f_open(&f, "/BMC64-DIAG.TXT", FA_WRITE | FA_OPEN_APPEND) == FR_OK) {
      unsigned int written;
      f_write(&f, line, (unsigned)n, &written);
      f_close(&f);
    }
  }

  for (;;) {

  }
}



static void stream_drop_cltbl(CircleFile &file) {
#if FF_USE_FASTSEEK
  file.file.cltbl = nullptr;
#endif
  if (file.cltbl) {
    free(file.cltbl);
    file.cltbl = nullptr;
  }
}






static void stream_setup(CircleFile &file) {
  file.streamed = 1;
  file.size = (unsigned)f_size(&file.file);
  printf("new_io: %s a flusso, %u byte\n", file.fname, file.size);

#if FF_USE_FASTSEEK




  DWORD n = 64;
  for (int tentativo = 0; tentativo < 2; tentativo++) {
    DWORD *t = (DWORD *)realloc(file.cltbl, n * sizeof(DWORD));
    if (t == nullptr) {
      break;
    }
    file.cltbl = t;
    file.cltbl[0] = n;
    file.file.cltbl = file.cltbl;
    FRESULT r = f_lseek(&file.file, CREATE_LINKMAP);
    if (r == FR_OK) {
      return;
    }
    if (r == FR_NOT_ENOUGH_CORE && file.cltbl[0] > n && file.cltbl[0] <= 4096) {
      n = file.cltbl[0];
      continue;
    }
    break;
  }
  stream_drop_cltbl(file);
#endif
}

extern "C" int _open(char *file, int flags, int mode) {
  SchedaInUso guardia("_open");
  bmc_scheda_file = file;
  (void) mode;
  int const masked_flags = flags & 7;
  if (masked_flags != O_RDONLY && masked_flags != O_WRONLY &&
      masked_flags != O_RDWR) {
    errno = ENOSYS;
    return -1;
  }

  // Handle fast fail here
  for (int i=0;i<g_bootStatNum;i++) {
     if (g_bootStatWhat[i] == BOOTSTAT_WHAT_FAIL) {
        if (strend(file, g_bootStatFile[i])) {
          errno = EACCES;
          return -1;
        }
     }
  }
  int slot = FindFreeFileSlot();

  if (slot != -1) {
    CirclePath circlePath(file);
    CircleFile &newFile = fileTab[slot];

    if (circlePath.troppo_lungo) {
      errno = ENAMETOOLONG;
      return -1;
    }








    int const in_coda = (flags & O_APPEND) ? 1 : 0;
    int result;
    if (masked_flags == O_RDONLY) {
      result = f_open(&newFile.file, circlePath.path, FA_READ);
    } else if (masked_flags == O_WRONLY) {
      result = f_open(&newFile.file, circlePath.path,
         in_coda ? (FA_WRITE | FA_OPEN_APPEND)
                 : (FA_WRITE | FA_CREATE_ALWAYS));
    } else {
      assert(masked_flags == O_RDWR);
      BYTE come = FA_READ | FA_WRITE;
      if (flags & O_TRUNC) {
        come |= FA_CREATE_ALWAYS;
      } else if (flags & O_CREAT) {
        come |= FA_OPEN_ALWAYS;
      }
      result = f_open(&newFile.file, circlePath.path, come);
    }

    if (result != FR_OK) {
      errno = EACCES;
      return -1;
    }

    newFile.fopen_called = 1;
    newFile.contents = nullptr;
    newFile.position = 0;
    newFile.size = 0;
    newFile.allocated = 0;
    newFile.mode = masked_flags;
    newFile.written_to = 0;
    newFile.riversato_fino_a = 0;
    newFile.da_sincronizzare = 0;
    newFile.streamed = 0;
    newFile.cltbl = nullptr;
    newFile.in_coda = in_coda;



    newFile.coda_da = (in_coda && masked_flags == O_WRONLY)
                    ? (unsigned)f_size(&newFile.file) : 0;
    strcpy(newFile.fname, circlePath.path);







    if (masked_flags == O_RDWR) {
       unsigned sz = (unsigned)f_size(&newFile.file);
       if (sz >= STREAM_MAX_BYTES) {
          printf("_open: immagine troppo grande da indirizzare (>= 2 GB)\n");
          f_close(&newFile.file);
          errno = EFBIG;
          return -1;
       }
       if (sz > SLURP_MAX_BYTES || e_un_dischetto(newFile.fname) ||
           e_una_reu(newFile.fname)) {


          stream_setup(newFile);
       } else {
          int r = slurp_file(newFile);
          if (r == -2) {

             stream_setup(newFile);
          } else if (r != 0) {
             f_close(&newFile.file);
             errno = ENFILE;
             return -1;
          }
       }
    }

    newFile.in_use = 1;
  } else {
    errno = ENFILE;
    printf("[FT] TABELLA DEI FILE PIENA, non si apre %s\n", file);
    circle_file_aperti(1);
  }

  return slot;
}

extern "C" int _close(int fildes) {
  SchedaInUso guardia("_close");
  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    errno = EBADF;
    return -1;
  }

  CircleFile &file = fileTab[fildes];
  bmc_scheda_file = file.fname;
  int esito = 0;
  if (!file.in_use) {
    errno = EBADF;
    return -1;
  }

    if (file.contents) {
      if (file.mode == O_WRONLY) {






        unsigned int num_written = 0;
        unsigned int da_scrivere = file.size > file.riversato_fino_a
                                 ? file.size - file.riversato_fino_a : 0;
        if (da_scrivere > 0 &&
            (f_write(&file.file, file.contents + file.riversato_fino_a,
                      da_scrivere, &num_written) != FR_OK ||
             num_written != da_scrivere)) {
           esito = -1;
        }
     }
  }

  int need_close = file.fopen_called;
  int errore_scrittura = esito;

  file.allocated = 0;
  file.size = 0;
  file.mode = 0;
  file.in_use = 0;
  file.written_to = 0;
  file.riversato_fino_a = 0;
  file.da_sincronizzare = 0;
  file.fopen_called = 0;
  file.streamed = 0;
  file.in_coda = 0;
  file.coda_da = 0;
  file.fname[0] = '\0';

  if (file.contents) {
    free(file.contents);
    file.contents = nullptr;
  }

  stream_drop_cltbl(file);
  
  
  if (need_close && f_close(&file.file) != FR_OK) {
    errno = EIO;
    return -1;
  }

  if (errore_scrittura) {
    errno = EIO;
    return -1;
  }

  return 0;
}







extern "C" int circle_file_da_sincronizzare(int fildes) {
  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    return 0;
  }
  CircleFile &file = fileTab[fildes];
  return file.in_use && file.fopen_called && file.da_sincronizzare;
}

extern "C" int fsync(int fildes) {
  SchedaInUso guardia("fsync");
  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    errno = EBADF;
    return -1;
  }

  CircleFile &file = fileTab[fildes];
  bmc_scheda_file = file.fname;
  if (!file.in_use || !file.fopen_called) {
    errno = EBADF;
    return -1;
  }









  if (file.mode == O_WRONLY && file.contents != nullptr &&
      file.size > file.riversato_fino_a) {
    unsigned int quanti = file.size - file.riversato_fino_a;
    unsigned int scritti = 0;
    if (f_write(&file.file, file.contents + file.riversato_fino_a,
                quanti, &scritti) != FR_OK || scritti != quanti) {
      errno = EIO;
      return -1;
    }
    file.riversato_fino_a = file.size;
  }

  if (f_sync(&file.file) != FR_OK) {
    errno = EIO;
    return -1;
  }
  file.da_sincronizzare = 0;

  return 0;
}












static int decidi_come_tenerlo(CircleFile &file) {
  unsigned sz;

  if (file.mode != O_RDONLY || file.contents != nullptr || file.streamed) {
    return 0;
  }
  sz = (unsigned)f_size(&file.file);
  if (sz >= STREAM_MAX_BYTES) {
    errno = EFBIG;
    return -1;
  }
  if (sz > SLURP_MAX_BYTES || e_un_dischetto(file.fname) ||
      e_una_reu(file.fname)) {


    stream_setup(file);
    return 0;
  }
  {
    int r = slurp_file(file);
    if (r == -2) {


      stream_setup(file);
      return 0;
    }
    if (r != 0) {
      errno = EACCES;
      return -1;
    }
  }
  return 0;
}

extern "C" int _read(int fildes, char *ptr, int len) {
  SchedaInUso guardia("_read");
  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    errno = EBADF;
    return -1;
  }

  CircleFile &file = fileTab[fildes];
  bmc_scheda_file = file.fname;
  if (!file.in_use) {
    errno = EBADF;
    return -1;
  }

  unsigned int num_read;




  if (decidi_come_tenerlo(file) != 0) {
    return -1;
  }

  if (file.contents == nullptr) {
     // Assert file.FIL has been opened
     // else EBADF -1

     if (file.streamed) {




       if (file.position >= file.size) {

         return 0;
       }
       if (f_tell(&file.file) != file.position) {
         f_lseek(&file.file, file.position);
       }
     }

     // Read data from the file
     if (f_read(&file.file, ptr, len, &num_read) != FR_OK) {
       errno = EIO;
       return -1;
     }

     file.position += num_read;
     return static_cast<int>(num_read);
  } else {
     // Read data from our internal buffer
     unsigned int max = len;
     unsigned int remain;



     if (file.position >= file.size) {
        return 0;
     }
     remain = file.size - file.position;

     if (max > remain) {
        max = remain;
     }

     if (max > 0) {
        memcpy(ptr, file.contents + file.position, max);
        file.position += max;
     }
     return static_cast<int>(max);
  }
}









#define BOOT_RIGHE_MAX 12288
#define BOOT_RIGA_MAX 480
static char s_boot_righe[BOOT_RIGHE_MAX];
static unsigned s_boot_n = 0;
static unsigned s_boot_scritti = 0;
static int s_boot_piene = 0;
static char s_boot_riga[CORES][BOOT_RIGA_MAX];
static unsigned s_boot_riga_n[CORES];
static unsigned char s_boot_riga_lunga[CORES];
static CSpinLock s_boot_blocco;









#define V3D_RIGHE 8
static char s_v3d_riga[V3D_RIGHE][BOOT_RIGA_MAX + 5];
static unsigned s_v3d_tot = 0;
static unsigned s_v3d_scritte = 0;









#define ULTIME_RIGHE 24
#define ULTIMA_RIGA_MAX 160
static char s_ultime[ULTIME_RIGHE][ULTIMA_RIGA_MAX];
static unsigned s_ultime_tot = 0;

static unsigned metti_numero(char *d, unsigned long v, unsigned cifre) {
  char t[24];
  unsigned n = 0;
  do {
    t[n++] = (char)('0' + v % 10);
    v /= 10;
  } while (v != 0 && n < sizeof(t));
  unsigned k = 0;
  while (n + k < cifre) {
    d[k++] = ' ';
  }
  while (n > 0) {
    d[k++] = t[--n];
  }
  return k;
}

static void ultima_riga_tieni(unsigned c, const char *r, unsigned n) {
  if (n == 0) {
    return;
  }
  char *u = s_ultime[s_ultime_tot % ULTIME_RIGHE];
  unsigned k = metti_numero(u, (unsigned long)(CTimer::GetClockTicks64() /
                                               1000U), 8);
  u[k++] = ' ';
  u[k++] = 'c';
  u[k++] = (char)('0' + c % 10);
  u[k++] = ' ';
  unsigned resta = ULTIMA_RIGA_MAX - 1 - k;
  if (n > resta) {
    n = resta;
  }
  memcpy(u + k, r, n);
  u[k + n] = '\0';
  s_ultime_tot++;
}


extern "C" unsigned bmc_ultime_righe(char *d, unsigned spazio) {
  static char copia[ULTIME_RIGHE][ULTIMA_RIGA_MAX];
  s_boot_blocco.Acquire();
  const unsigned tot = s_ultime_tot;
  const unsigned quante = tot < ULTIME_RIGHE ? tot : ULTIME_RIGHE;
  for (unsigned i = 0; i < quante; i++) {
    memcpy(copia[i], s_ultime[(tot - quante + i) % ULTIME_RIGHE],
           ULTIMA_RIGA_MAX);
  }
  s_boot_blocco.Release();
  unsigned k = 0;
  for (unsigned i = 0; i < quante && spazio > 0; i++) {
    const unsigned n = (unsigned)strlen(copia[i]);
    if (k + n + 3 > spazio) {
      break;
    }
    memcpy(d + k, copia[i], n);
    k += n;
    d[k++] = '\r';
    d[k++] = '\n';
  }
  if (k < spazio) {
    d[k] = '\0';
  }
  return k;
}

static int riga_contiene(const char *r, unsigned n, const char *cosa) {
  const unsigned m = (unsigned)strlen(cosa);
  for (unsigned i = 0; i + m <= n; i++) {
    if (memcmp(r + i, cosa, m) == 0) {
      return 1;
    }
  }
  return 0;
}

static void boot_riga_chiudi(unsigned c) {
  const unsigned n = s_boot_riga_n[c];
  const int lunga = s_boot_riga_lunga[c];
  s_boot_riga_n[c] = 0;
  s_boot_riga_lunga[c] = 0;
  s_boot_blocco.Acquire();
  ultima_riga_tieni(c, s_boot_riga[c], n);
  s_boot_blocco.Release();
  if (n < 6 || memcmp(s_boot_riga[c], "boot: ", 6) != 0) {
    return;
  }
  if (riga_contiene(s_boot_riga[c], n, "v3d stats") ||
      riga_contiene(s_boot_riga[c], n, "bmc64 v3d ")) {
    s_boot_blocco.Acquire();
    char *d = s_v3d_riga[s_v3d_tot % V3D_RIGHE];
    memcpy(d, s_boot_riga[c], n);
    if (lunga) {
      memcpy(d + n, " ...", 4);
      d[n + 4] = '\0';
    } else {
      d[n] = '\0';
    }
    s_v3d_tot++;
    s_boot_blocco.Release();
  }
  const unsigned serve = n + (lunga ? 4 : 0) + 1;
  s_boot_blocco.Acquire();
  if (s_boot_n + serve <= BOOT_RIGHE_MAX) {
    memcpy(s_boot_righe + s_boot_n, s_boot_riga[c], n);
    if (lunga) {
      memcpy(s_boot_righe + s_boot_n + n, " ...", 4);
    }
    s_boot_righe[s_boot_n + serve - 1] = '\n';
    s_boot_n += serve;
  } else {
    s_boot_piene = 1;
  }
  s_boot_blocco.Release();
}

static void boot_righe_raccogli(const char *p, int len) {
  const unsigned c = CMultiCoreSupport::ThisCore() % CORES;
  for (int i = 0; i < len; i++) {
    const char ch = p[i];
    if (ch == '\r') {
      continue;
    }
    if (ch == '\n') {
      boot_riga_chiudi(c);
      continue;
    }
    const unsigned n = s_boot_riga_n[c];
    if (n < BOOT_RIGA_MAX) {
      s_boot_riga[c][n] = ch;
      s_boot_riga_n[c] = n + 1;
    } else {
      s_boot_riga_lunga[c] = 1;
    }
  }
}




extern "C" void bmc_righe_boot_scrivi(FILE *fp) {
  static char copia[BOOT_RIGHE_MAX];
  s_boot_blocco.Acquire();
  const unsigned da = s_boot_scritti;
  const unsigned a = s_boot_n;
  const int piene = s_boot_piene;
  memcpy(copia, s_boot_righe + da, a - da);
  s_boot_scritti = a;
  s_boot_blocco.Release();
  if (a > da) {
    fwrite(copia, 1, a - da, fp);
  } else {
    fprintf(fp, "   (nessuna riga nuova)\n");
  }
  if (piene) {
    fprintf(fp, "   (lo spazio per le righe e' finito: quelle dopo non ci sono)\n");
  }


  static char copia_v3d[V3D_RIGHE][BOOT_RIGA_MAX + 5];
  s_boot_blocco.Acquire();
  unsigned vda = s_v3d_scritte;
  const unsigned va = s_v3d_tot;
  if (va - vda > V3D_RIGHE) {
    vda = va - V3D_RIGHE;
  }
  for (unsigned k = vda; k < va; k++) {
    memcpy(copia_v3d[k - vda], s_v3d_riga[k % V3D_RIGHE], BOOT_RIGA_MAX + 5);
  }
  s_v3d_scritte = va;
  s_boot_blocco.Release();
  if (va > vda) {
    fprintf(fp, "   -- la V3D: tempi e scelte (%u righe, le ultime) --\n",
            va - vda);
    for (unsigned k = 0; k < va - vda; k++) {
      fprintf(fp, "%s\n", copia_v3d[k]);
    }
  }
}



extern "C" unsigned bmc_righe_boot_nuove(void) {
  s_boot_blocco.Acquire();
  const unsigned nuove = s_boot_n - s_boot_scritti +
                         (s_v3d_tot - s_v3d_scritte);
  s_boot_blocco.Release();
  return nuove;
}

extern "C" int _write(int fildes, char *ptr, int len) {





  if (fildes == 1 || fildes == 2) {
    boot_righe_raccogli(ptr, len);
    if (g_serial) {
       return g_serial->Write(ptr, len);
    } 
    return len;
  }

  SchedaInUso guardia("_write");
  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    errno = EBADF;
    return -1;
  }

  CircleFile &file = fileTab[fildes];
  bmc_scheda_file = file.fname;
  if (!file.in_use) {
    errno = EBADF;
    return -1;
  }

  // Keep the RAM cache coherent with the on-disk image.
  file.written_to = 1;

  file.da_sincronizzare = 1;


  if (file.in_coda) {
    file.position = file.size;
  }





  if (file.streamed) {
     unsigned int num_written = 0;
     if (f_lseek(&file.file, file.position) != FR_OK ||
         f_write(&file.file, ptr, len, &num_written) != FR_OK ||
         num_written != static_cast<unsigned int>(len)) {
       errno = EIO;
       return -1;
     }
     file.position += len;
     if (file.position > file.size) {




        file.size = file.position;
        stream_drop_cltbl(file);
     }
     return len;
  }

  unsigned int write_position = file.position;
  unsigned int dimensione_prima = file.size;







  if (write_position < file.riversato_fino_a) {
    file.riversato_fino_a = 0;
    f_lseek(&file.file, 0);
  }

  // Nothing allocated yet? Allocate now.
  if (file.contents == nullptr) {
     file.allocated = WRITE_BUF_SIZE;
     file.contents = (char *) malloc(file.allocated);
     if (file.contents == nullptr) {
        file.allocated = 0;
        errno = ENOMEM;
        return -1;
     }
  }

  // Make sure we always have enough room allocated for the



  while (file.position + len >= (unsigned)file.allocated) {
     char *bigger = (char *)realloc(file.contents, file.allocated * 2);
     if (bigger == nullptr) {
        errno = ENOMEM;
        return -1;
     }
     file.contents = bigger;
     file.allocated *= 2;
  }



  if (file.position > file.size) {
     memset(file.contents + file.size, 0, file.position - file.size);
  }

  // Do the write.
  memcpy(file.contents + file.position, ptr, len);
  file.position += len;
  if (file.position > file.size) {
     file.size = file.position;
  }

  if (file.mode == O_RDWR) {
     unsigned int num_written = 0;







     if (write_position > dimensione_prima) {
        static const char zeri[512] = { 0 };
        unsigned int da = dimensione_prima;
        if (f_lseek(&file.file, da) != FR_OK) {
          errno = EIO;
          return -1;
        }
        while (da < write_position) {
          unsigned int n = write_position - da;
          unsigned int scritti = 0;
          if (n > sizeof(zeri)) {
            n = sizeof(zeri);
          }
          if (f_write(&file.file, zeri, n, &scritti) != FR_OK || scritti != n) {
            errno = EIO;
            return -1;
          }
          da += n;
        }
     }

     if (f_lseek(&file.file, write_position) != FR_OK ||
         f_write(&file.file, ptr, len, &num_written) != FR_OK ||
         num_written != static_cast<unsigned int>(len)) {
       errno = EIO;
       return -1;
     }
  }

  return len;
}

extern "C" int _isatty(int fildes) {
  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    errno = EBADF;
    return -1;
  }

  CircleFile &file = fileTab[fildes];
  if (!file.in_use) {
    errno = EBADF;
    return -1;
  }

  return (fildes == 0 || fildes == 1 || fildes == 2) ? 1 : 0;
}

extern "C" int _fcntl(int fildes, int cmd, int arg) {
  (void) arg;

  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    errno = EBADF;
    return -1;
  }

  CircleFile &file = fileTab[fildes];
  if (!file.in_use) {
    errno = EBADF;
    return -1;
  }

  switch (cmd) {
  case F_GETFL:
    if (file.mode != 0) {
      return file.mode;
    }
    if (fildes == 0) {
      return O_RDONLY;
    }
    if (fildes == 1 || fildes == 2) {
      return O_WRONLY;
    }
    return 0;

  case F_SETFL:
#ifdef F_GETFD
  case F_GETFD:
#endif
#ifdef F_SETFD
  case F_SETFD:
#endif
    return 0;

  default:
    errno = EINVAL;
    return -1;
  }
}

extern "C" DIR *opendir(const char *name) {
  SchedaInUso guardia("opendir");
  bmc_scheda_file = name;
  CirclePath circlePath(name);
  if (circlePath.troppo_lungo) {
    errno = ENAMETOOLONG;
    return 0;
  }
  
  int const slotNum = FindFreeDirSlot();
  if (slotNum == -1) {
    errno = ENFILE;
    return 0;
  }

  CircleDir &slot = dirTab[slotNum];
  if (f_opendir(&slot.dir.mCurrentEntry, circlePath.path) != FR_OK) {
    errno = ENFILE;
    return 0;
  }

  slot.dir.mOpen = 1;
  slot.dir.mFirstRead = 1;
  slot.in_use = 1;
  return &slot.dir;
}

static bool read_next_entry(FATFS_DIR *currentEntry, FILINFO *filinfo) {
  if (f_readdir(currentEntry, filinfo) == FR_OK) {
    return filinfo->fname[0] != 0;
  }

  errno = EBADF;
  return false;
}

static struct dirent *do_readdir(DIR *dir, struct dirent *de) {
  assert(dir->mOpen);

  FILINFO fno;
  bool haveEntry;
  struct dirent *result = nullptr;

  if (dir->mFirstRead) {
    if (f_readdir(&dir->mCurrentEntry, nullptr) == FR_OK) {
      haveEntry = read_next_entry(&dir->mCurrentEntry, &fno);
    } else {
      errno = EBADF;
      haveEntry = false;
    }
    dir->mFirstRead = 0;
  } else {
    haveEntry = read_next_entry(&dir->mCurrentEntry, &fno);
  }

  if (haveEntry) {
    strcpy(de->d_name, fno.fname);
    de->d_ino = 0;




    de->d_type = (fno.fattrib & AM_DIR) ? DT_DIR : DT_REG;
    result = de;
  }

  return result;
}

extern "C" struct dirent *readdir(DIR *dir) {
  SchedaInUso guardia("readdir");
  CircleDir *c_dir = FindCircleDirFromDIR(dir);
  if (c_dir == nullptr) {
    errno = EBADF;
    return nullptr;
  }

  if (!dir->mOpen) {
    errno = EBADF;
    return nullptr;
  }

  return do_readdir(dir, &dir->mEntry);
}

extern "C" int readdir_r(DIR *__restrict dir, dirent *__restrict de,
                         dirent **__restrict ode) {
  int result;
  CircleDir *c_dir = FindCircleDirFromDIR(dir);

  if (c_dir == nullptr) {
    *ode = nullptr;
    result = EBADF;
  } else if (!dir->mOpen) {
    *ode = nullptr;
    result = EBADF;
  } else {
    *ode = do_readdir(dir, de);
    result = 0;
  }

  return result;
}

extern "C" void rewinddir(DIR *dir) {
  dir->mFirstRead = 1;
}

extern "C" int closedir(DIR *dir) {
  SchedaInUso guardia("closedir");
  CircleDir *c_dir = FindCircleDirFromDIR(dir);
  if (c_dir == nullptr) {
    errno = EBADF;
    return -1;
  }

  if (!dir->mOpen) {
    errno = EBADF;
    return -1;
  }

  c_dir->in_use = 0;
  dir->mOpen = 0;

  if (f_closedir(&dir->mCurrentEntry) != FR_OK) {
    errno = EIO;
    return -1;
  }

  return 0;
}

extern "C" int _stat(const char *file, struct stat *st) {
  SchedaInUso guardia("_stat");
  bmc_scheda_file = file;
  CirclePath circlePath(file);
  memset(st, 0, sizeof(struct stat));
  if (circlePath.troppo_lungo) {
    errno = ENAMETOOLONG;
    return -1;
  }

  // Fastfail or fastsucceed
  for (int i=0;i<g_bootStatNum;i++) {
     if (g_bootStatWhat[i] == BOOTSTAT_WHAT_STAT) {






        if (strend(circlePath.path, g_bootStatFile[i])) {
          st->st_mode = S_IFREG | S_IRUSR | S_IWUSR;
          st->st_size = g_bootStatSize[i];
          return 0;
        }
     }
     else if (g_bootStatWhat[i] == BOOTSTAT_WHAT_FAIL) {
        if (strend(circlePath.path, g_bootStatFile[i])) {
          errno = EBADF;
          return -1;
        }
     }
  }

  FILINFO fno;
  if (f_stat(circlePath.path, &fno) == FR_OK) {
    if (fno.fattrib & AM_DIR) {
      st->st_mode |= S_IFDIR;
    } else {
      st->st_mode |= S_IFREG;
    }
    if (fno.fattrib & AM_RDO) {
      st->st_mode |= S_IRUSR;
    } else {
      st->st_mode |= S_IRUSR | S_IWUSR;
    }

    st->st_size = fno.fsize;
    return 0;
  }

  errno = EBADF;
  return -1;
}

extern "C" int _fstat(int fildes, struct stat *st) {

  CircleFile &file = fileTab[fildes];
  if (!file.in_use) {
    errno = EBADF;
    return -1;
  }

  return _stat(file.fname, st);
}

extern "C" int _lseek(int fildes, int ptr, int dir) {
  SchedaInUso guardia("_lseek");

  if (fildes < 0 || static_cast<unsigned int>(fildes) >= MAX_OPEN_FILES) {
    errno = EBADF;
    return -1;
  }

  CircleFile &file = fileTab[fildes];
  bmc_scheda_file = file.fname;
  if (!file.in_use) {
    errno = EBADF;
    return -1;
  }



  if (decidi_come_tenerlo(file) != 0) {
    return -1;
  }

  long where;


  long const base = (file.mode == O_WRONLY && file.in_coda)
                  ? (long)file.coda_da : 0;

  if (dir == SEEK_SET) {
    where = ptr;
  } else if (dir == SEEK_CUR) {
    where = base + (long)file.position + ptr;
  } else if (dir == SEEK_END) {
    where = base + (long)file.size + ptr;
  } else {
    errno = EINVAL;
    return -1;
  }







  if (where < 0) {
    errno = EINVAL;
    return -1;
  }

  file.position = where > base ? (unsigned)(where - base) : 0;





  if (file.streamed && file.position <= file.size &&
      f_tell(&file.file) != file.position) {
    f_lseek(&file.file, file.position);
  }

  return (int)(base + (long)file.position);
}

extern "C" int chdir (const char *path)
{
  int i;

  if (path == nullptr) {
     errno = EIO;
     return -1;
  }

  int len = strlen(path);
  if (len == 0) {
     return 0;
  }

  if (len == 1 && path[0] == '.') {
     return 0;
  }

  // Up to parent
  if (len == 2 && path[0] == '.' && path[1] == '.') {
     if (strlen(currentDir) == 0) {
        return 0;
     }
     if (strlen(currentDir) == 1 && currentDir[0] == '/') {
        return 0;
     }
     for (i=strlen(currentDir)-1; i >= 0; i--) {
        if (currentDir[i] == '/') {
           currentDir[i] = '\0';
           return 0;
        }
     }
     return 0;
  }

  CirclePath circlePath(path);
  if (circlePath.troppo_lungo) {
     errno = ENAMETOOLONG;
     return -1;
  }
  if (path[0] == '/') {
     // Absolute
     strcpy(currentDir, circlePath.path);
  } else {

     size_t n = strlen(currentDir);
     int barra = (n == 0 || currentDir[n-1] != '/') ? 1 : 0;
     if (n + barra + strlen(circlePath.path) >= sizeof(currentDir)) {
        errno = ENAMETOOLONG;
        return -1;
     }
     if (barra) {
        strcat(currentDir, "/");
     }
     strcat(currentDir, circlePath.path);
  }

  return 0;
}

extern "C" char *getwd(char *buf) {
   if (buf) {
      strcpy(buf, currentDir);
      if (strlen(buf) > 1 && buf[strlen(buf)-1] == '/') {
         buf[strlen(buf)-1] = '\0';
      }
   }
   return buf;
}

extern "C" int access(const char *fn, int flags)
{
  struct stat st;

  if (stat(fn, &st) != 0) {
    return -1;
  }

  if (st.st_mode & S_IFDIR) {
    return 0;
  }

  if ((flags & W_OK) != 0 && (st.st_mode & S_IWUSR) == 0) {
    errno = EACCES;
    return -1;
  }

  return 0;
}

extern "C" int _link(char *existing, char *newname) {
  SchedaInUso guardia("_link");
  bmc_scheda_file = existing;
  int result = f_rename(existing, newname);
  if (result != FR_OK) {
     if (result == FR_EXIST) errno = EEXIST;
     else errno = EBADF;
     return -1;
  }
  return 0;
}

extern "C" int _unlink(char *name) {
  SchedaInUso guardia("_unlink");
  bmc_scheda_file = name;
  f_unlink(name);
  return 0;
}
