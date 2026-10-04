#
# Makefile
#

TOPDIR = ../..

include $(TOPDIR)/Config.mk

MONGOOSE_DIR = $(TOPDIR)/libs/mongoose

OBJS	= main.o kernel.o mongoose.o buttontask.o

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE) -I $(TOPDIR)/libs/json/single_include -I $(MONGOOSE_DIR) -I.

CFLAGS += -DMG_ARCH=0

mongoose.o: $(MONGOOSE_DIR)/mongoose.c $(MONGOOSE_DIR)/mongoose.h
	$(CC) $(CFLAGS) $(INCLUDE) -c -o $@ $<

LIBS := $(CIRCLE_STDLIB_LIBS)

-include $(DEPS)
