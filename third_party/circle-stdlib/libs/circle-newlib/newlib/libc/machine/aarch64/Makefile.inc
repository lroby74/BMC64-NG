#	mem functions disabled in libc to avoid duplicate functions with circle
#	%D%/memmove.c
#	%D%/memcpy-stub.c
#	%D%/memcpy.S
#	%D%/memset-stub.c
#	%D%/memset.S

libc_a_SOURCES += \
	%D%/memchr-stub.c \
	%D%/memchr.S \
	%D%/memcmp-stub.c \
	%D%/memcmp.S \
	%D%/memrchr-stub.c \
	%D%/memrchr.S \
	%D%/rawmemchr.S \
	%D%/rawmemchr-stub.c \
	%D%/setjmp.S \
	%D%/stpcpy-stub.c \
	%D%/stpcpy.S \
	%D%/strchr-stub.c \
	%D%/strchr.S \
	%D%/strchrnul-stub.c \
	%D%/strchrnul.S \
	%D%/strcmp-stub.c \
	%D%/strcmp.S \
	%D%/strcpy-stub.c \
	%D%/strcpy.S \
	%D%/strlen-stub.c \
	%D%/strlen.S \
	%D%/strncmp-stub.c \
	%D%/strncmp.S \
	%D%/strnlen-stub.c \
	%D%/strnlen.S \
	%D%/strrchr-stub.c \
	%D%/strrchr.S
