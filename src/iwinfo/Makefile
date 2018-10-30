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

# Wrapper makefile for iwinfo build

MODULE := iwinfo
VERSION ?= $(MODULE)-dev
REPO_URL := git://git.openwrt.org/project/$(MODULE).git
BUILD_PATH := $(TARGET_OBJ)/$(MODULE)
SOURCE_PATH := $(BUILD_PATH)/$(VERSION)

all: install

install: .installed

build: .compiled

.PHONY: all clean build install

clean:
	rm .configured .compiled .installed
	rm -rf $(SOURCE_PATH)

$(SOURCE_PATH):
	git clone $(REPO_URL) $(SOURCE_PATH)

.configured: $(SOURCE_PATH)
# By default, the build system creates links to pre-existing sources.
# We can't patch the symlinks, so instead copy the source code directly
# TODO: a bit wasteful, just apply the patches in situ?
	if [ ! -d $(SOURCE_PATH)/.git ]; then \
		cd "$(SOURCE_PATH)" && find * -type l -exec rm -fv {} \; && \
		cd "$(TOP)/src/$(MODULE)/$(VERSION)" && \
			find * -type f -exec \
				cp -avf "$(TOP)/src/$(MODULE)/$(VERSION)/{}" "$(SOURCE_PATH)/{}" \; ; \
	fi
	cd $(SOURCE_PATH) && \
		patch -st < $(BUILD_PATH)/patches/300-iwinfo-ubuntu-gnu-no-lua.patch
	touch $@

.compiled: .configured
	make -C $(SOURCE_PATH) BACKENDS="nl80211" FPIC="-fPIC"
	touch $@ $^

.installed: .compiled
	mkdir -p $(TARGET_OBJ)/lib
	mkdir -p $(TARGET_OBJ)/include
	cp $(SOURCE_PATH)/libiwinfo.so $(TARGET_OBJ)/lib
	cp -r $(SOURCE_PATH)/include/* $(TARGET_OBJ)/include
	touch $@
