# (c) 2017-2018 minim.co
# Makefile for platform unum utility functions library

# Add model specific include path to make util.h 
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/util/$(MODEL)

# Include common portion of the makefile for the subsystem
include ./util/util_common.mk

# If crash info collector code is available for the 
# specific hardware being built pull the file in.
ifneq (,$(wildcard ./util/$(MODEL)/util_crashinfo_$(HARDWARE).c))
  OBJECTS += ./util/$(MODEL)/util_crashinfo_$(HARDWARE).o
endif
