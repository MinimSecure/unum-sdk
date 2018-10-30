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

# Wrapper makefile for libnl-tiny build

MODULE := libnl-tiny
VERSION ?= $(MODULE)-dev
REPO_URL := https://github.com/openwrt/openwrt/trunk/package/libs/libnl-tiny/src
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
	svn export $(REPO_URL) $(SOURCE_PATH)
	mkdir $(SOURCE_PATH)/.git

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
		patch -st < $(TARGET_OBJ)/$(MODULE)/patches/000-libnl-fpic.patch
	touch $@

.compiled: .configured
	$(MAKE) -C $(SOURCE_PATH) all
	touch $@ $^

.installed: .compiled
	mkdir -p $(TARGET_OBJ)/lib $(TARGET_OBJ)/include
	cp $(SOURCE_PATH)/libnl-tiny.so $(TARGET_OBJ)/lib
	sed -e 's:/usr:$(TARGET_OBJ):g' $(BUILD_PATH)/libnl-tiny.pc > $(TARGET_OBJ)/lib/libnl-tiny.pc
	cp -r $(SOURCE_PATH)/include/* $(TARGET_OBJ)/include
	touch $@
