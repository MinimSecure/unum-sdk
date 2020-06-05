# (c) 2017-2019 minim.co
# Plaform independent part of Makefile for support portal agent

# Add model specific include path to make support.h 
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/support/$(MODEL)

# Add code file(s)
OBJECTS += ./support/support.o

# Enable support portal agent mode of operation
CPPFLAGS += -DSUPPORT_RUN_MODE
