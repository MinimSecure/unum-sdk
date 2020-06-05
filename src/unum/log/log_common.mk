# (c) 2017-2019 minim.co
# Plaform independet part of Makefile for the logging subsystem

# Add model specific include path for the logging subfolder.
# It allows to pull in platform specific include file.
CPPFLAGS += -I$(UNUM_PATH)/log/$(MODEL)

# Add logging code file(s)
OBJECTS += ./log/log.o \
           ./log/$(MODEL)/log_platform.o

# Add logging subsystem initializer function
INITLIST += log_init
