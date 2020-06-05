# (c) 2019 minim.co
# Makefile for Asus MAP's unum utility functions library

# Add model specific include path to make util.h 
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/util/$(MODEL)

# Include common portion of the makefile for the subsystem
include ./util/util_common.mk

# Add crash info collector routines
OBJECTS += ./util/util_crashinfo_arm.o
