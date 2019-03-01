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

# Makefile for building Unum for Linux.

# Build-time path where original, unmodified copies of the files distributed
# with the Unum agent binary will be placed.
TARGET_RFS_DIST := $(TARGET_RFS)/dist

# Runtime configuration and variable file paths.
TARGET_RFS_ETC := /etc/opt/unum
TARGET_RFS_VAR := /var/opt/unum

# Include "extras", a collection of utilities for common platforms.
INSTALL_EXTRAS ?= 1
INSTALL_EXTRAS := $(filter-out no n 0,$(INSTALL_EXTRAS))

####################################################################
# Common platform build options                                    #
####################################################################

TARGET_LIBS := $(TARGET_OBJ)/lib/
LD_PATH_PREFIX := $(TARGET_OBJ)/lib/
PKG_CONFIG_PATH := $(TARGET_OBJ)/lib/

CC := cc
CXX := c++
GCC := gcc
LD := ld
STRIP := strip

TARGET_CFLAGS += \
	-I$(TARGET_OBJ)/include/

####################################################################
# Lists of components to build and install for the platform        #
####################################################################

TARGET_LIST := iwinfo unum

# Static files bundled with the binary (default config files, etc)
# are handled in the `files.install` and `extras.install` targets.
TARGET_INSTALL_LIST := $(TARGET_LIST) files

# Conditionally include the "extras" target.
ifneq ($(INSTALL_EXTRAS),)
	TARGET_INSTALL_LIST += extras
endif

####################################################################
# Additional flags and vars for building the specific components   #
####################################################################

### iwinfo
IWINFO_VERSION := iwinfo-c3044fbbe525cca36e4f0fb9ce0f257710cc6834
TARGET_CFLAGS_iwinfo := -Wall -D_GNU_SOURCE -I/usr/include/libnl3
TARGET_LDFLAGS_iwinfo := -lnl-3 -lnl-genl-3
TARGET_VARS_iwinfo := \
 	VERSION=$(IWINFO_VERSION) \
	BACKENDS=nl80211

### unum
TARGET_VARS_unum := \
	IWINFO=$(IWINFO_VERSION) \
	UNUM_DISABLE_TRACER=1
TARGET_CPPFLAGS_unum := \
	-D'PERSISTENT_FS_DIR_PATH=\"$(TARGET_RFS_VAR)\"' \
	-D'LOG_PATH_PREFIX=\"$(TARGET_RFS_VAR)\"' \
	-D'ETC_PATH_PREFIX=\"$(TARGET_RFS_ETC)\"'

# Modify the read and apply config scripts to use the "extras" implementation.
ifneq ($(INSTALL_EXTRAS),)
	TARGET_CPPFLAGS_unum += -D'PLATFORM_CONFIG_READ_SCRIPT=\"/opt/unum/extras/sbin/read_conf.sh\"' \
						    -D'PLATFORM_CONFIG_APPLY_SCRIPT=\"/opt/unum/extras/sbin/apply_conf.sh\"'
endif

# Component dependencies
unum: iwinfo

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
	cp -r -f "$(TARGET_FILES)/etc/"* "$(TARGET_RFS_DIST)$(TARGET_RFS_ETC)"
	mkdir -p "$(TARGET_RFS)/sbin"
	cp -r -f "$(TARGET_FILES)/sbin/"* "$(TARGET_RFS)/sbin"

extras.install:
	mkdir -p "$(TARGET_RFS)/extras"
	cp -r -f $(TOP)/extras/$(MODEL)/etc $(TOP)/extras/$(MODEL)/sbin "$(TARGET_RFS)/extras"
	cp -f "$(TOP)/extras/$(MODEL)/install.sh" "$(TARGET_RFS)/extras/install.sh"
