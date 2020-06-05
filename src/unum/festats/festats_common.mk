# (c) 2018-2019 minim.co
# common makefile for forwarding engine stats subsystem

CPPFLAGS += -I$(UNUM_PATH)/festats/$(MODEL)

OBJECTS += \
	./festats/festats.o       \
	./festats/$(MODEL)/festats_platform.o
