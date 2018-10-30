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

# Wrapper makefile for uci build

MODULE := uci
VERSION ?= $(MODULE)-dev
REPO_URL := git://git.openwrt.org/project/$(MODULE).git
BUILD_PATH := $(TARGET_OBJ)/$(MODULE)
SOURCE_PATH := $(BUILD_PATH)/$(VERSION)
UCI_FILE_PATH ?= /etc/config

all: install

install: .installed

build: .compiled

.PHONY: all clean build install

clean:
	rm .configured .compiled .installed
	rm -rf $(SOURCE_PATH)

$(SOURCE_PATH):
	git clone $(REPO_URL) $(SOURCE_PATH)

$(SOURCE_PATH)/uci.h.orig: $(SOURCE_PATH)
	mv "$(SOURCE_PATH)/uci.h" "$(SOURCE_PATH)/uci.h.orig"

.configured: $(SOURCE_PATH) $(SOURCE_PATH)/uci.h.orig
	sed -e 's:^#define UCI_CONFDIR .*$$:#define UCI_CONFDIR "$(UCI_FILE_PATH)":' \
		"$(SOURCE_PATH)/uci.h.orig" > "$(SOURCE_PATH)/uci.h"
	cd $(SOURCE_PATH) && \
	cmake -DBUILD_LUA=OFF -DBUILD_STATIC=ON \
		  -DCMAKE_INSTALL_PREFIX:PATH=$(TARGET_OBJ)
	touch $@

.compiled: .configured
	$(MAKE) -C $(SOURCE_PATH)
	touch $@ $^

.installed: .compiled
	$(MAKE) -C $(SOURCE_PATH) install
	touch $@
