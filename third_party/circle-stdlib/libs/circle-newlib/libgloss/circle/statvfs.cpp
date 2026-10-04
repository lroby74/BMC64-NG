#include <sys/statvfs.h>

extern "C" int fstatvfs(int, struct statvfs *) { return -1; }

extern "C" int statvfs(const char *, struct statvfs *) {
    int res = -1;

    return res;
}