#
# Makefile
#

include ../../Config.mk

OBJS	= main.o kernel.o

include $(CIRCLEHOME)/Rules.mk

INCLUDE := $(CIRCLE_STDLIB_INCLUDES) $(INCLUDE)

LIBS := $(CIRCLE_STDLIB_LIBS)

-include $(DEPS)
