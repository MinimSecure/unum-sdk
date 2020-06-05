# (c) 2017 minim.co
# Makefile for majority of MTK based platforms ieee802.11 functionality

# Add model specific include path to make wireless.h
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/wireless/$(MODEL) -I$(UNUM_PATH)/wireless/mt76x0

# Add code file(s)
OBJECTS += ./wireless/wireless_extensions.o                    \
           ./wireless/mt76xx_common/mt76xx_radios_platform.o   \
           ./wireless/mt76xx_common/mt76xx_stas_platform.o     \
           ./wireless/mt76xx_common/mt76xx_assoc_platform.o    \
           ./wireless/mt76xx_common/mt76xx_wireless_platform.o \
           ./wireless/$(MODEL)/scan_platform.o                 \
           ./wireless/$(MODEL)/wireless_platform.o

# Include common portion of the makefile for the subsystem
include ./wireless/wireless_common.mk
