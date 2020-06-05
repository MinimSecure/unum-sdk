# (c) 2019 minim.co
# Makefile for ieee802.11 functionality for iwinfo based platforms

# Add model specific include path to make wireless.h
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/wireless/$(MODEL)

# Add code file(s)
OBJECTS += ./wireless/$(MODEL)/radios_platform.o \
           ./wireless/$(MODEL)/stas_platform.o   \
           ./wireless/$(MODEL)/scan_platform.o   \
           ./wireless/wireless_iwinfo.o          \
           ./wireless/wireless_iwinfo_qca.o          \
           ./wireless/$(MODEL)/wireless_platform.o   \
           ./wireless/$(MODEL)/wireless_platform_qca.o

# Include common portion of the makefile for the subsystem
include ./wireless/wireless_common.mk
