# (c) 2017 minim.co
# Wrapper makefile for mbedtls libraries build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/mbedtls/$(SUBDIR)
CFLAGS += -I. $(CPPFLAGS)

all: .compiled

.compiled: $(wildcard $(MYSRCDIR)/**) Makefile
	ln -sf ../../../config-$(MODEL).h $(SUBDIR)/include/mbedtls/config.h
	cd "$(SUBDIR)" && $(MAKE) SHARED=1 no_test
	# Make lib folder with libs for dependent packages to refer to
	cd "$(SUBDIR)" && mkdir -p ./lib
	cd "$(SUBDIR)/lib" && cp -fP ../library/*.so ../library/*.so.* ./
	touch $@
