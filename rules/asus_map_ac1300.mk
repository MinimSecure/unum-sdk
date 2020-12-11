# (c) 2019-2020 minim.co
# Makefile for compiling unum and other ASUS MAP 1300 components

TARGET_TOOLCHAIN := /opt/openwrt-gcc463.arm/

export STAGING_DIR=$(TARGET_TOOLCHAIN)

CC := arm-openwrt-linux-uclibcgnueabi-gcc
#CCLD := ld
CXX := arm-openwrt-linux-uclibcgnueabi-c++
GCC := arm-openwrt-linux-uclibcgnueabi-gcc
#LD := ld
STRIP := "$(TARGET_TOOLCHAIN)/bin/arm-openwrt-linux-uclibcgnueabi-strip"

TARGET_PLATFORM := arm
TARGET_PLATFORM_KARCH := arm
TARGET_PLATFORM_ARCH := arm-openwrt-linux-uclibcgnueabi


# Additional binary and libs search paths for the platform build toolchain
PATH_PREFIX=$(TARGET_TOOLCHAIN)/bin:$(TARGET_TOOLCHAIN)/$(TARGET_PLATFORM_ARCH)/bin:$(TARGET_TOOLCHAIN)/libexec/gcc/$(TARGET_PLATFORM_ARCH)/4.6.3

TARGET_CPPFLAGS := \
	-I$(TARGET_TOOLCHAIN)/lib/gcc/$(TARGET_PLATFORM_ARCH)/4.6.3/include \
	-I$(TARGET_TOOLCHAIN)/libexec/gcc/$(TARGET_PLATFORM_ARCH)/4.6.3/include \
	-I$(TARGET_TOOLCHAIN)/lib/gcc/$(TARGET_PLATFORM_ARCH)/4.6.3/include-fixed \
	-I$(TARGET_TOOLCHAIN)/lib/gcc/$(TARGET_PLATFORM_ARCH)/4.6.3/include \
	-I$(TARGET_TOOLCHAIN)/$(TARGET_PLATFORM_ARCH)/include/c++/4.6.3 \
	-I$(TARGET_TOOLCHAIN)/$(TARGET_PLATFORM_ARCH)/sys-include

TARGET_CFLAGS := \
	-pipe -funit-at-a-time -Wno-pointer-sign -marm


####################################################################
# Lists of components to build and install for the platform        #
####################################################################

TARGET_LIST := iwinfo jansson unum # gdb
TARGET_INSTALL_LIST := $(TARGET_LIST) files

####################################################################


# Additional flags and vars for building the specific components

### iwinfo
IWINFO_VERSION := iwinfo-qca
TARGET_VARS_iwinfo := VERSION=$(IWINFO_VERSION)
IWINFO_BACKENDS := madwifi
IWINFO_MADWIFI_PATCH := 1

### jansson
JANSSON_VERSION := jansson-2.7
TARGET_VARS_jansson := VERSION=$(JANSSON_VERSION)

### unum
TARGET_CPPFLAGS_unum := \
	-I$(TARGET_LIBS)/libnvram/include \
	-I$(TARGET_LIBS)/libshared/include \
	-I$(TARGET_LIBS)/libssl/include \
	-I$(TARGET_LIBS)/libcurl/include \
	-I$(TARGET_LIBS)/libz/include

TARGET_LDFLAGS_unum := -ldl -lm -lrt \
	-L$(TARGET_LIBS)/libcurl/lib/ -l:libcurl.so \
	-L$(TARGET_LIBS)/libssl/lib/ -l:libssl.so \
	-L$(TARGET_LIBS)/libssl/lib/ -l:libcrypto.so \
	-L$(TARGET_LIBS)/libnvram/lib/ -l:libnvram.so \
        -L$(TARGET_LIBS)/libshared/lib/ -l:libshared.so \
        -L$(TARGET_LIBS)/libz/lib/ -l:libz.so
        
### GDB
GDB_VERSION := gdb-7.11
TARGET_VARS_gdb := VERSION=$(GDB_VERSION)

TARGET_VARS_unum := CARES=$(C-ARES_VERSION) \
                    IWINFO=$(IWINFO_VERSION) \
                    JANSSON=$(JANSSON_VERSION) 


# Component dependencies on each other
unum: iwinfo

jansson.install:
	mkdir -p "$(TARGET_RFS)/lib"
	$(STRIP) -o "$(TARGET_RFS)/lib/libjansson.so.4.7.0" "$(TARGET_OBJ)/jansson/$(JANSSON_VERSION)/src/.libs/libjansson.so.4.7.0"
	ln -sf "libjansson.so.4.7.0" "$(TARGET_RFS)/lib/libjansson.so"
	ln -sf "libjansson.so.4.7.0" "$(TARGET_RFS)/lib/libjansson.so.4"

iwinfo.install:
	mkdir -p "$(TARGET_RFS)/lib" "$(TARGET_RFS)/bin"
	$(STRIP) -o "$(TARGET_RFS)/lib/libiwinfo.so" "$(TARGET_OBJ)/iwinfo/$(IWINFO_VERSION)/libiwinfo.so"
	$(STRIP) -o "$(TARGET_RFS)/bin/iwinfo" "$(TARGET_OBJ)/iwinfo/$(IWINFO_VERSION)/iwinfo"

unum.install:
	mkdir -p "$(TARGET_RFS)/usr/bin"
	$(STRIP) -o "$(TARGET_RFS)/usr/bin/unum" "$(TARGET_OBJ)/unum/unum"

files.install:
	mkdir -p "$(TARGET_RFS)/etc/ssl/certs"
	mkdir -p "$(TARGET_RFS)/www" "$(TARGET_RFS)/sbin"
	mkdir -p "$(TARGET_RFS)/usr/bin" "$(TARGET_RFS)/.ssh"
	# Install custom config and model files
	# Trusted CA list
	cat "$(COMMON_FILES)/ca/"*.pem > "$(TARGET_RFS)/etc/ssl/certs/ca-certificates.crt"
	cp -f "$(COMMON_FILES)"/ssh_keys/* "$(TARGET_RFS)/.ssh/"
	echo $(AGENT_VERSION) > "$(TARGET_RFS)/etc/version"
	# Helper scripts
	cp -f "$(TARGET_FILES)/scripts/fw_upgrade.sh" "$(TARGET_RFS)/usr/bin/"
	cp -f "$(TARGET_FILES)/scripts/apply_config.sh" "$(TARGET_RFS)/sbin/"
	ln -sf /var/provision_info.htm $(TARGET_RFS)/www/provision_info.htm
