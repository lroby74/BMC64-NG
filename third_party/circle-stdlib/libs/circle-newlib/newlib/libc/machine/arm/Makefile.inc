# mem functions disabled in libc to avoid duplicate functions with Circle
#	%D%/aeabi_memcpy.c %D%/aeabi_memcpy-armv7a.S
#	%D%/aeabi_memmove.c %D%/aeabi_memmove-soft.S
#	%D%/aeabi_memset.c %D%/aeabi_memset-soft.S %D%/aeabi_memclr.c
#	%D%/memcpy-stub.c
#	%D%/memcpy.S

libc_a_SOURCES += \
	%D%/setjmp.S %D%/strcmp.S %D%/strcpy.c \
	%D%/memchr-stub.c \
	%D%/memchr.S \
	%D%/strlen-stub.c \
	%D%/strlen.S
