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
REPO_URL := https://github.com/MinimSecure/iwinfo-lite
BUILD_PATH := $(TARGET_OBJ)/$(MODULE)
SOURCE_PATH := $(BUILD_PATH)/$(VERSION)
VERSION_REV := $(patsubst iwinfo-%,%,$(VERSION))
ifeq ($(VERSION_REV),dev)
	VERSION_REV := master
endif

all: install

install: .installed

build: .compiled

.PHONY: all clean build install

clean:
	rm .configured .compiled .installed
	rm -rf $(SOURCE_PATH)

$(SOURCE_PATH):
	git clone --depth=1 $(REPO_URL) $(SOURCE_PATH)
	git -C $(SOURCE_PATH) checkout $(VERSION_REV)

.configured: $(SOURCE_PATH)
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
