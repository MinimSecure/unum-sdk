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

# Makefile for building Unum for linux.

# Define the directory where original copies
TARGET_RFS_DIST := $(TARGET_RFS)/dist
# Additionally define the host-specific configuration and variable file paths.
TARGET_RFS_ETC := /etc/opt/unum
TARGET_RFS_VAR := /var/opt/unum

TARGET_CA_PATH := $(TARGET_RFS_ETC)/ca-certificates.crt
TARGET_CA_SOURCES := $(wildcard $(TOP)/files/ca/*)

####################################################################
# Common platform build options                                    #
####################################################################

# Pre-validation is complete, carry on with makin' stuff.
TARGET_LIBS := $(TARGET_OBJ)/lib/
LD_PATH_PREFIX := $(TARGET_OBJ)/lib/
PKG_CONFIG_PATH := $(TARGET_OBJ)/lib/

TARGET_PLATFORM=
TARGET_PLATFORM_ARCH=x86_64-pc-linux-gnu

CC := cc
CXX := c++
GCC := gcc
LD := ld
STRIP := strip

TARGET_CFLAGS += -I$(TARGET_OBJ)/include/

####################################################################
# Lists of components to build and install for the platform        #
####################################################################

TARGET_LIST := \
	mbedtls curl jansson json-c libubox libnl-tiny ubus uci iwinfo unum

# Static files and initial runtime configuration is handled in the
# `files.install` target defined below.
TARGET_INSTALL_LIST := $(TARGET_LIST) files extras

####################################################################
# Additional flags and vars for building the specific components   #
####################################################################

### MbedTLS
MBEDTLS_VERSION := mbedtls-2.4.2
TARGET_VARS_mbedtls := VERSION=$(MBEDTLS_VERSION)

### curl
CURL_VERSION := curl-7.50.0
TARGET_VARS_curl := VERSION=$(CURL_VERSION) MBEDTLS=$(MBEDTLS_VERSION) CALIST=$(TARGET_CA_PATH)

### jansson
JANSSON_VERSION := jansson-2.7
TARGET_VARS_jansson := VERSION=$(JANSSON_VERSION)

### json-c
JSONC_VERSION := json-c-0.13.1
TARGET_VARS_json-c := VERSION=$(JSONC_VERSION)

### libubox
LIBUBOX_VERSION := libubox-c83a84a
TARGET_VARS_libubox := VERSION=$(LIBUBOX_VERSION)

### libnl-tiny
LIBNL_TINY_VERSION := libnl-tiny-d5bcd02
TARGET_VARS_libnl-tiny := VERSION=$(LIBNL_TINY_VERSION)

### ubus
UBUS_VERSION := ubus-0327a91
TARGET_VARS_ubus := VERSION=$(UBUS_VERSION)

### uci
UCI_VERSION := uci-4c8b4d6
TARGET_VARS_uci := VERSION=$(UCI_VERSION) UCI_FILE_PATH=$(TARGET_RFS_ETC)/config

### iwinfo
IWINFO_VERSION := iwinfo-77a9e98
TARGET_VARS_iwinfo := VERSION=$(IWINFO_VERSION)

### unum
TARGET_VARS_unum := \
	MBEDTLS=$(MBEDTLS_VERSION) CURL=$(CURL_VERSION) JANSSON=$(JANSSON_VERSION) \
	UCI=$(UCI_VERSION) IWINFO=$(IWINFO_VERSION) \
	PERSISTENT_FS_DIR_PATH="$(TARGET_RFS_VAR)" \
	LOG_PATH_PREFIX="$(TARGET_RFS_VAR)/log" \
	ETC_PATH_PREFIX="$(TARGET_RFS_ETC)"

# Component dependencies
unum: curl jansson iwinfo
curl: mbedtls
uci: ubus
ubus: libubox
libubox: json-c
iwinfo: uci libnl-tiny

mbedtls.install:
	mkdir -p "$(TARGET_RFS)/lib"
	$(STRIP) -o "$(TARGET_RFS)/lib/libmbedcrypto.so.0" "$(TARGET_OBJ)/mbedtls/$(MBEDTLS_VERSION)/library/libmbedcrypto.so.0"
	$(STRIP) -o "$(TARGET_RFS)/lib/libmbedtls.so.10" "$(TARGET_OBJ)/mbedtls/$(MBEDTLS_VERSION)/library/libmbedtls.so.10"
	$(STRIP) -o "$(TARGET_RFS)/lib/libmbedx509.so.0" "$(TARGET_OBJ)/mbedtls/$(MBEDTLS_VERSION)/library/libmbedx509.so.0"
	ln -sf "libmbedcrypto.so.0" "$(TARGET_RFS)/lib/libmbedcrypto.so"
	ln -sf "libmbedtls.so.10" "$(TARGET_RFS)/lib/libmbedtls.so"
	ln -sf "libmbedx509.so.0" "$(TARGET_RFS)/lib/libmbedx509.so"

curl.install:
	mkdir -p "$(TARGET_RFS)/lib" "$(TARGET_RFS)/bin"
	$(STRIP) -o "$(TARGET_RFS)/lib/libcurl.so.4.4.0" "$(TARGET_OBJ)/curl/$(CURL_VERSION)/lib/.libs/libcurl.so.4.4.0"
	ln -sf "libcurl.so.4.4.0" "$(TARGET_RFS)/lib/libcurl.so"
	ln -sf "libcurl.so.4.4.0" "$(TARGET_RFS)/lib/libcurl.so.4"
	$(STRIP) -o "$(TARGET_RFS)/bin/curl" "$(TARGET_OBJ)/curl/$(CURL_VERSION)/src/.libs/curl"

jansson.install:
	mkdir -p "$(TARGET_RFS)/lib"
	$(STRIP) -o "$(TARGET_RFS)/lib/libjansson.so.4.7.0" "$(TARGET_OBJ)/jansson/$(JANSSON_VERSION)/src/.libs/libjansson.so.4.7.0"
	ln -sf "libjansson.so.4.7.0" "$(TARGET_RFS)/lib/libjansson.so"
	ln -sf "libjansson.so.4.7.0" "$(TARGET_RFS)/lib/libjansson.so.4"

json-c.install:
	mkdir -p "$(TARGET_RFS)/lib"
	$(STRIP) -o "$(TARGET_RFS)/lib/libjson-c.so.4.0.0" "$(TARGET_OBJ)/lib/libjson-c.so.4.0.0"
	ln -sf "libjson-c.so.4.0.0" "$(TARGET_RFS)/lib/libjson-c.so"
	ln -sf "libjson-c.so.4.0.0" "$(TARGET_RFS)/lib/libjson-c.so.4"
	$(STRIP) -o "$(TARGET_RFS)/lib/libblobmsg_json.so" "$(TARGET_OBJ)/lib/libblobmsg_json.so"

libubox.install:
	mkdir -p "$(TARGET_RFS)/lib"
	$(STRIP) -o "$(TARGET_RFS)/lib/libblobmsg_json.so" "$(TARGET_OBJ)/lib/libblobmsg_json.so"
	$(STRIP) -o "$(TARGET_RFS)/lib/libjson_script.so" "$(TARGET_OBJ)/lib/libjson_script.so"
	$(STRIP) -o "$(TARGET_RFS)/lib/libubox.so" "$(TARGET_OBJ)/lib/libubox.so"

libnl-tiny.install:
	mkdir -p "$(TARGET_RFS)/lib"
	$(STRIP) -o "$(TARGET_RFS)/lib/libnl-tiny.so" "$(TARGET_OBJ)/libnl-tiny/$(LIBNL_TINY_VERSION)/libnl-tiny.so"

ubus.install:
	mkdir -p "$(TARGET_RFS)/lib"
	$(STRIP) -o "$(TARGET_RFS)/lib/libubus.so" "$(TARGET_OBJ)/ubus/$(UBUS_VERSION)/libubus.so"

uci.install:
	mkdir -p "$(TARGET_RFS)/lib" "$(TARGET_RFS)/bin"
	$(STRIP) -o "$(TARGET_RFS)/lib/libuci.so" "$(TARGET_OBJ)/uci/$(UCI_VERSION)/libuci.so"
	$(STRIP) -o "$(TARGET_RFS)/bin/uci" "$(TARGET_OBJ)/uci/$(UCI_VERSION)/uci"

iwinfo.install:
	mkdir -p "$(TARGET_RFS)/lib" "$(TARGET_RFS)/bin"
	$(STRIP) -o "$(TARGET_RFS)/lib/libiwinfo.so" "$(TARGET_OBJ)/iwinfo/$(IWINFO_VERSION)/libiwinfo.so"
	$(STRIP) -o "$(TARGET_RFS)/bin/iwinfo" "$(TARGET_OBJ)/iwinfo/$(IWINFO_VERSION)/iwinfo"

unum.install:
	mkdir -p "$(TARGET_RFS)/bin"
	echo "$(AGENT_VERSION)" > $(TARGET_RFS)/version
	$(STRIP) -o "$(TARGET_RFS)/bin/unum" "$(TARGET_OBJ)/unum/unum"

files.install:
	mkdir -p "$(TARGET_RFS_DIST)$(TARGET_RFS_ETC)" "$(TARGET_RFS_DIST)$(TARGET_RFS_VAR)"
	cat $(TARGET_CA_SOURCES) > "$(TARGET_RFS_DIST)$(TARGET_CA_PATH)"
	cp -r -f "$(TARGET_FILES)/etc/defaults" "$(TARGET_RFS_DIST)$(TARGET_RFS_ETC)/defaults"

extras.install:
	mkdir -p "$(TARGET_RFS)/extras/sbin"
	cp -r -f $(TOP)/extras/$(MODEL)/etc $(TOP)/extras/$(MODEL)/sbin "$(TARGET_RFS)/extras"
	cp -f "$(TARGET_OBJ)/uci/$(UCI_VERSION)/sh/uci.sh" "$(TARGET_RFS)/extras/sbin/uci.sh"
	cp -f "$(TOP)/extras/$(MODEL)/install_extras.sh" "$(TARGET_RFS)/extras/install_extras.sh"
