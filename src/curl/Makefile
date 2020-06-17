# (c) 2017-2020 minim.co
# Wrapper makefile for curl library build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/curl/$(SUBDIR)
CPPFLAGS += -I.

ifneq ($(MBEDTLS),)
  CONFIG_OPTIONS := --with-mbedtls=$(TARGET_OBJ)/mbedtls/$(MBEDTLS) \
                    --without-ca-path --with-ca-bundle=$(CALIST)
  LDFLAGS += -Wl,-rpath-link,$(TARGET_OBJ)/mbedtls/$(MBEDTLS)/lib
  CPPFLAGS += -I$(TARGET_OBJ)/mbedtls/$(MBEDTLS)/include/mbedtls
endif

ifneq ($(OPENSSL),)
  LIBS := "-lssl -lcrypto"
  CONFIG_OPTIONS :=  --with-ssl=$(TARGET_LIBS)/libssl \
                   --without-ca-path --with-ca-bundle=$(CALIST)
  LDFLAGS += -Wl,-rpath-link,$(TARGET_LIBS)/libssl/lib/
endif
ifneq ($(CARES),)
  LDFLAGS += -L$(TARGET_OBJ)/c-ares/$(CARES)/.libs/
  LDFLAGS += -Wl,-rpath-link,$(TARGET_OBJ)/c-ares/$(CARES)/.libs/
  CPPFLAGS += -I$(TARGET_OBJ)/c-ares/$(CARES)/
  CONFIG_OPTIONS += --enable-ares=$(TARGET_OBJ)/c-ares/$(CARES)/
else
  CONFIG_OPTIONS += --disable-ares
endif

all: .compiled

.compiled: $(wildcard $(MYSRCDIR)/**) .configured Makefile
	cd "$(SUBDIR)" && $(MAKE)
	touch $@


.configured: Makefile $(MYSRCDIR)/configure $(MYSRCDIR)/configure.ac $(MYSRCDIR)/Makefile.in
	cd "$(SUBDIR)" && \
	./configure --build=x86_64-pc-linux-gnu --host=$(TARGET_PLATFORM_ARCH) \
                    --disable-debug --enable-shared --disable-manual \
                    --without-nss --without-libmetalink --without-librtmp \
                    --without-axtls --without-cyassl --without-gnutls --without-polarssl \
                    --without-libidn --without-libssh2 --without-zlib \
                    --disable-ipv6 --enable-pthreads \
                    --disable-dict --disable-ftp --enable-file --disable-gopher \
                    --enable-http --enable-https --disable-imap --disable-ldap --disable-ldaps \
                    --disable-pop3 --disable-rtsp --disable-smb --disable-smtp \
                    --disable-telnet --disable-tftp --disable-cookies --disable-crypto-auth \
                    --disable-libcurl-option --disable-proxy --disable-threaded-resolver \
                    --disable-tls-srp --disable-unix-sockets --disable-verbose \
                    $(CONFIG_OPTIONS)
	touch $@

