# (c) 2020 minim.co
# Wrapper makefile for dropbear build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/dropbear/$(SUBDIR)
CPPFLAGS += -Wall -I.

all: .compiled

.PHONY: checkoptions

# Dropbear requires clean if options change
.compiled:: options_$(MODEL).h .configured
	ln -sf ../options_$(MODEL).h $(SUBDIR)/options.h
	cd "$(SUBDIR)" && $(MAKE) clean

.compiled:: $(wildcard $(MYSRCDIR)/**) options_$(MODEL).h .configured Makefile
	cd "$(SUBDIR)" && $(MAKE) PROGRAMS="dropbear dbclient dropbearkey scp" MULTI=1
	touch $@

.configured: Makefile $(MYSRCDIR)/configure $(MYSRCDIR)/configure.ac $(MYSRCDIR)/Makefile.in
	cd "$(SUBDIR)" && \
        ./configure --build=x86_64-pc-linux-gnu --host=$(TARGET_PLATFORM_ARCH) \
                    --disable-zlib --disable-syslog --disable-lastlog \
                    --disable-utmp --disable-utmpx --disable-wtmp --disable-wtmpx \
	            $(EXTRA_CONFIG_OPTS)
	touch $@
