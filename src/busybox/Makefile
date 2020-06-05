# (c) 2017 minim.co
# Wrapper makefile for building busybox

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYOBJDIR := $(TARGET_OBJ)/busybox/$(SUBDIR)
MYSRCDIR := $(TOP)/src/busybox/$(SUBDIR)

all: .compiled

.compiled: Makefile config-$(MODEL) $(wildcard $(MYSRCDIR)/**)
	ln -sf ../config-$(MODEL) $(MYOBJDIR)/.config
	cd "$(SUBDIR)" && PATH=$$HOSTCCPATH:$$PATH $(MAKE) V=1
	touch $@

