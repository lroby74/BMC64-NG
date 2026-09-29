

AM_CPPFLAGS = \
	@ARCH_INCLUDES@ \
	$(extra_includes) \
	-I$(top_builddir)/src \
	-I$(top_srcdir)/src/drive \
	-I$(top_srcdir)/src/c64 \
	-I$(top_srcdir)/src/c64/cart \
	-I$(top_srcdir)/src/c64dtv \
	-I$(top_srcdir)/src/c128 \
	-I$(top_srcdir)/src/vic20 \
	-I$(top_srcdir)/src/pet \
	-I$(top_srcdir)/src/cbm2 \
	-I$(top_srcdir)/src/plus4 \
	-I$(top_srcdir)/src/raster \
	-I$(top_srcdir)/src/rs232drv \
	-I$(top_srcdir)/src/samplerdrv \
	-I$(top_srcdir)/src/sid \
	-I$(top_srcdir)/src/userport \
	-I$(top_srcdir)/src/vdrive \
	-I$(top_srcdir)/src/video \
	-I$(top_srcdir)/src/lib/p64 \
	-I$(top_srcdir)/src/platform \
	-I$(top_srcdir)/src/joyport \
	-I$(top_srcdir)/src/gfxoutputdrv \
	-I$(top_srcdir)/src/tapeport \
	-I$(top_srcdir)/src/imagecontents \
	-I$(top_srcdir)/src

noinst_LIBRARIES = libarch_c64dtv.a

libarch_c64dtv_a_SOURCES = \
	archdep_c64dtv.h \
	archdep_c64dtv.c \
	videoarch_c64dtv.h \
	videoarch_c64dtv.c \
	virtual_keyboard_c64dtv.h \
	virtual_keyboard_c64dtv.c \
	menu_c64dtv.h \
	menu_c64dtv.c \
	userport_c64dtv.h \
	userport_c64dtv.c
