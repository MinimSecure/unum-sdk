# (c) 2018 minim.co
# Wrapper makefile for iwinfo build

MODULE := libiw
VERSION ?= $(MODULE)-dev
BUILD_PATH := $(TARGET_OBJ)/$(MODULE)
SOURCE_PATH := $(BUILD_PATH)/$(VERSION)

libiw:
	$(GCC) $(CFLAGS) $(TARGET_CPPFLAGS_unum) -Ilibiw_wext29/include -c iwlib.c
	$(GCC) $(CFLAGS) $(LDFLAGS) -shared -o libiw.so iwlib.o

all: install
