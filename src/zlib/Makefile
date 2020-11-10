# (c) 2020 minim.co
# Wrapper makefile for zlib library

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/zlib/$(SUBDIR)
CPPFLAGS += -I.

all: .compiled

.compiled: $(wildcard $(MYSRCDIR)/**) .configured Makefile
	cd "$(SUBDIR)" && $(MAKE)
	touch $@

.configured: Makefile $(MYSRCDIR)/configure $(MYSRCDIR)/Makefile.in
	cd "$(SUBDIR)" && \
        ./configure \
                    $(CONFIG_OPTIONS)
	touch $@
