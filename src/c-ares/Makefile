# (c) 2019 minim.co
# Wrapper makefile for c-ares library build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/c-ares/$(SUBDIR)
CPPFLAGS += -I.

all: .compiled

.compiled: $(wildcard $(MYSRCDIR)/**) .configured Makefile
	cd "$(SUBDIR)" && $(MAKE)
	touch $@

.configured: Makefile $(MYSRCDIR)/configure $(MYSRCDIR)/Makefile.in
	cd "$(SUBDIR)" && \
        ./configure --build=x86_64-pc-linux-gnu --host=$(TARGET_PLATFORM_ARCH) \
                    --disable-ipv6 --enable-pthreads \
                    $(CONFIG_OPTIONS)
	touch $@
