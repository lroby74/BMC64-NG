#
# Makefile
#

include $(TOPDIR)/Config.mk

CIRCLEHOME = $(TOPDIR)/libs/circle
NEWLIBDIR = $(TOPDIR)/install/$(NEWLIB_ARCH)

OBJS	= main.o

include $(CIRCLEHOME)/Rules.mk

INCLUDE += $(CIRCLE_STDLIB_INCLUDES) -I $(TOPDIR)/tests/lib -I $(TOPDIR)/libs/doctest
LIBS := "$(TOPDIR)/tests/lib/libdoctest.a" $(CIRCLE_STDLIB_LIBS)
