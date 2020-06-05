# (c) 2017 minim.co
# Wrapper makefile for gdbserver build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYOBJDIR := $(TARGET_OBJ)/gdb
MYSRCDIR := $(TOP)/src/gdb
GDBSRVOBJDIR := $(MYOBJDIR)/gdbserver
GDBSRVSRCDIR := $(TOP)/src/gdb/$(SUBDIR)/gdb/gdbserver
CFLAGS += -Wall -I.

all: .compiled

.compiled: .configured $(wildcard $(MYSRCDIR)/**)
	cd "$(GDBSRVOBJDIR)" && $(MAKE)
	touch $@

.configured: Makefile $(GDBSRVSRCDIR)/configure $(GDBSRVSRCDIR)/configure.ac $(GDBSRVSRCDIR)/Makefile.in
	mkdir -p "$(GDBSRVOBJDIR)"
	cd "$(GDBSRVOBJDIR)" && \
          $(GDBSRVSRCDIR)/configure  --host $(TARGET_PLATFORM_ARCH) --target $(TARGET_PLATFORM_ARCH)
	touch $@
