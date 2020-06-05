# (c) 2017 minim.co
# Wrapper makefile for jansson library build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/jansson/$(SUBDIR)
CPPFLAGS += -I.

all: .compiled

.compiled: $(wildcard $(MYSRCDIR)/**) .configured Makefile
	cd "$(SUBDIR)" && $(MAKE)
	touch $@

.configured: Makefile $(MYSRCDIR)/configure $(MYSRCDIR)/configure.ac $(MYSRCDIR)/Makefile.in
	cd "$(SUBDIR)" && \
	./configure --build=x86_64-pc-linux-gnu --host=$(TARGET_PLATFORM_ARCH)
	touch $@
