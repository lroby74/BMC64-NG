#include "semaphore.h"

#if defined(__aarch64__)






void sem_dec(uint32_t* semaphore) {
    uint32_t value;
    uint32_t failed;
    asm volatile (
      "1: ldaxr   %w0, [%2]\n"
      "   cbz     %w0, 2f\n"
      "   sub     %w0, %w0, #1\n"
      "   stxr    %w1, %w0, [%2]\n"
      "   cbnz    %w1, 1b\n"
      "   dmb     ish\n"
      "   b       3f\n"
      "2: wfe\n"
      "   b       1b\n"
      "3:\n"
      : "=&r" (value), "=&r" (failed)
      : "r" (semaphore)
      : "memory", "cc");
}

void sem_inc(uint32_t* semaphore) {
    uint32_t value;
    uint32_t failed;
    asm volatile (
      "1: ldxr    %w0, [%2]\n"
      "   add     %w0, %w0, #1\n"
      "   stlxr   %w1, %w0, [%2]\n"
      "   cbnz    %w1, 1b\n"
      "   dsb     ish\n"
      "   sev\n"
      : "=&r" (value), "=&r" (failed)
      : "r" (semaphore)
      : "memory", "cc");
}

#else

void sem_dec(uint32_t* semaphore) {
#ifndef RASPI_LITE
    asm volatile (
      "1:  LDREX    r1, [r0]\n"
      "    CMP	    r1, #0\n"
      "    BEQ     2f             \n"
      "    SUB     r1, #1        \n"
      "    STREX   r2, r1, [r0]  \n"
      "    CMP     r2, #0        \n"
      "    BNE     1b            \n"
      "    DMB                   \n"
      "    B       3f\n"
      "2: \n" 
      "    wfe \n"
      "    B       1b\n"
      "3:\n"
    );
#endif
}

void sem_inc(uint32_t* semaphore) {
#ifndef RASPI_LITE
    asm volatile (
      "1:   LDREX   r1, [r0]\n"
      "     ADD     r1, #1\n"
      "     STREX   r2, r1, [r0]\n"
      "     CMP     r2, #0\n"
      "     BNE     1b\n"
      "     CMP     r0, #1\n"
      "     DMB\n"
      "     BGE     2f\n"
      "     B       3f\n"
      "2:\n"
      "     DSB\n"
      "     SEV\n"
      "3:\n"
    );
#endif
}

#endif
