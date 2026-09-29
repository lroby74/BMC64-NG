#ifndef BOOTSTAT_SCPU64
#define BOOTSTAT_SCPU64

int dflt_bootStatNum = 4;

int dflt_bootStatWhat[] = {
    BOOTSTAT_WHAT_STAT,
    BOOTSTAT_WHAT_STAT,
    BOOTSTAT_WHAT_FAIL,
    BOOTSTAT_WHAT_FAIL,
};

const char *dflt_bootStatFile[] = {
    "scpu64",
    "chargen-901225-01.bin",
    "fliplist-SCPU64.vfl",
    "dos2000",
};

int dflt_bootStatSize[] = {
    65536,
    4096,
    0,
    0
};

#endif
