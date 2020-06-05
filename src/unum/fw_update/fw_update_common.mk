# (c) 2017-2019 minim.co
# Plaform independent part of Makefile for unum firmware updater 

# Add model specific include path to make fw_update.h 
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/fw_update/$(MODEL)

# Add code file(s)
OBJECTS += ./fw_update/fw_update.o

# Enable firmware updater mode of operation
CPPFLAGS += -DFW_UPDATER_RUN_MODE

