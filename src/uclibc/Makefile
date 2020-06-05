# (c) 2017 minim.co
# Wrapper makefile for building uclibc

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYOBJDIR := $(TARGET_OBJ)/uclibc/$(SUBDIR)
MYSRCDIR := $(TOP)/src/uclibc/$(SUBDIR)

all: .compiled

.compiled: .configured
	cd "$(SUBDIR)" && $(MAKE) V=1 \
	    CROSS=$(strip $(TARGET_PLATFORM_ARCH))- \
	    KERNEL_HEADERS='$(KERNEL_HEADERS)' \
	    RUNTIME_PREFIX=/
	touch $@

.configured: Makefile config-$(MODEL) $(wildcard $(MYSRCDIR)/**)
	ln -sf ../config-$(MODEL) $(MYOBJDIR)/.config
	cd "$(SUBDIR)" && PATH=$$HOSTCCPATH:$$PATH $(MAKE) \
	  CROSS=$(strip $(TARGET_PLATFORM_ARCH))- \
	  KERNEL_HEADERS='$(KERNEL_HEADERS)' \
	  oldconfig
	touch $@

# ld-uClibc libc libcrypt libdl libm libnsl \
# libpthread libresolv librt libutil
