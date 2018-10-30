# Copyright 2018 Minim Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
endif

all: .compiled

$(SUBDIR):
	curl -L https://curl.haxx.se/download/$(VERSION).tar.gz > $(VERSION).tar.gz
	tar xf $(VERSION).tar.gz

.compiled: $(wildcard $(MYSRCDIR)/**) .configured Makefile
	cd "$(SUBDIR)" && $(MAKE)
	touch $@

.configured: $(SUBDIR) Makefile | $(SUBDIR)/configure $(SUBDIR)/configure.ac $(SUBDIR)/Makefile.in
	cd "$(SUBDIR)" && \
        ./configure --build=x86_64-pc-linux-gnu --host=$(TARGET_PLATFORM_ARCH) \
                    --disable-debug --disable-ares --enable-shared --disable-manual \
                    --without-nss --without-libmetalink --without-librtmp \
                    --without-axtls --without-cyassl --without-gnutls --without-ssl --without-polarssl \
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
