include ../circle-stdlib/Config.mk
CIRCLE_STDLIB_HOME = ../circle-stdlib
NEWLIB_ARCH ?= arm-none-circle

LIBSRC = aes.c aes_reference.c aes128ccm.c alloc.c asn1-ber.c compat.c libsmb2-dcerpc.c \
	libsmb2-dcerpc-srvsvc.c errors.c hmac.c hmac-md5.c init.c libsmb2.c md4c.c md5.c ntlmssp.c \
	pdu.c sha1.c sha224-256.c sha384-512.c smb2-cmd-cancel.c smb2-cmd-close.c smb2-cmd-create.c \
	smb2-cmd-echo.c smb2-cmd-error.c smb2-cmd-flush.c smb2-cmd-ioctl.c smb2-cmd-lock.c \
	smb2-cmd-logoff.c smb2-cmd-negotiate.c smb2-cmd-notify-change.c smb2-cmd-oplock-break.c \
	smb2-cmd-query-directory.c smb2-cmd-query-info.c smb2-cmd-read.c smb2-cmd-session-setup.c \
	smb2-cmd-set-info.c smb2-cmd-tree-connect.c smb2-cmd-tree-disconnect.c smb2-cmd-write.c \
	smb2-data-file-info.c smb2-data-filesystem-info.c smb2-data-security-descriptor.c \
	smb2-data-reparse-point.c smb2-share-enum.c smb3-seal.c smb2-signing.c socket.c spnego-wrapper.c \
	sync.c timestamps.c unicode.c usha.c

INCLUDES = -I . -I libsmb2/include -I libsmb2/include/smb2 -I libsmb2/lib -I src \
	-I $(CIRCLE_STDLIB_HOME)/install/$(NEWLIB_ARCH)/include \
	-I $(CIRCLE_STDLIB_HOME)/include \
	-I $(CIRCLE_STDLIB_HOME)/libs/circle/include \
	-I $(CIRCLE_STDLIB_HOME)/libs/circle/addon \
	-I $(CIRCLE_STDLIB_HOME)/libs/circle/addon/fatfs

DEFS = -DBMC_SOCK_CIRCLE -DHAVE_CONFIG_H -D_GNU_SOURCE "-D_U_=__attribute__((unused))" -DBMC_SOCK_IOVEC -DMD5Init=smb2_MD5Init -DMD5Update=smb2_MD5Update -DMD5Final=smb2_MD5Final

LIBOBJ = $(addprefix obj/lib_,$(LIBSRC:.c=.o))
OWNOBJ = obj/bmc_smb.o obj/bmc_sock.o obj/bmc_nomi.o obj/bmc_wsd.o obj/bmc_nas.o

all: libbmcsmb.a

obj:
	mkdir -p obj

obj/lib_%.o: libsmb2/lib/%.c config.h src/bmc_sock_compat.h src/bmc_sock.h | obj
	$(CC_FOR_TARGET) $(INCLUDES) $(DEFS) -include src/bmc_sock_compat.h -w -c -O2 -o $@ $< $(CFLAGS_FOR_TARGET)

obj/bmc_smb.o: src/bmc_smb.c src/bmc_smb.h | obj
	$(CC_FOR_TARGET) $(INCLUDES) $(DEFS) -Wall -Wno-unused-parameter -c -O2 -o $@ $< $(CFLAGS_FOR_TARGET)

obj/bmc_sock.o: src/bmc_sock.c src/bmc_sock.h | obj
	$(CC_FOR_TARGET) $(INCLUDES) $(DEFS) -Wall -c -O2 -o $@ $< $(CFLAGS_FOR_TARGET)

obj/bmc_nomi.o: src/bmc_nomi.c src/bmc_nomi.h | obj
	$(CC_FOR_TARGET) $(INCLUDES) $(DEFS) -Wall -c -O2 -o $@ $< $(CFLAGS_FOR_TARGET)

obj/bmc_wsd.o: src/bmc_wsd.c src/bmc_wsd.h src/bmc_nomi.h | obj
	$(CC_FOR_TARGET) $(INCLUDES) $(DEFS) -Wall -c -O2 -o $@ $< $(CFLAGS_FOR_TARGET)

obj/bmc_nas.o: src/bmc_nas.c src/bmc_nas.h | obj
	$(CC_FOR_TARGET) $(INCLUDES) $(DEFS) -Wall -c -O2 -o $@ $< $(CFLAGS_FOR_TARGET)

libbmcsmb.a: $(LIBOBJ) $(OWNOBJ)
	rm -f $@
	$(AR_FOR_TARGET) cr $@ $(LIBOBJ) $(OWNOBJ)
	$(RANLIB_FOR_TARGET) $@

clean:
	rm -rf obj libbmcsmb.a
