# (c) 2019 - 2021 minim.co
# Makefile for ieee802.11 functionality for iwinfo based platforms

# Add model specific include path to make wireless.h
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/wireless/$(MODEL)

nl80211 = $(shell echo $(RELEASE_DEFINES) | grep FEATURE_MAC80211_LIBNL)
# Add code file(s)
OBJECTS += ./wireless/$(MODEL)/radios_platform.o        \
           ./wireless/$(MODEL)/stas_platform.o          \
           ./wireless/$(MODEL)/wireless_platform.o      \
           ./wireless/wireless_iwinfo.o

ifeq ($(nl80211),)
  OBJECTS += ./wireless/$(MODEL)/scan_platform.o
else
  OBJECTS += ./wireless/wireless_nl80211.o              \
             ./wireless/wireless_nl80211_lede.o         \
             ./wireless/nl80211_platform/scan_platform.o
endif
# Include common portion of the makefile for the subsystem
include ./wireless/wireless_common.mk
