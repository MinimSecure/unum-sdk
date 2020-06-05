# (c) 2019 minim.co
# Plaform independent part of Makefile for unum firmware updater 

# Add model specific include path to make fw_update.h 
# come from the right place, but do not compile in or enable it
# since it is not used for the platform
CPPFLAGS += -I$(UNUM_PATH)/fw_update/$(MODEL)

