# (c) 2017 minim.co
# Plaform independent part of Makefile for ieee802.11 related functionality

# Add code file(s)
OBJECTS += ./wireless/wireless.o \
           ./wireless/wireless_stubs.o

# Add subsystem initializer function
INITLIST += wireless_init

