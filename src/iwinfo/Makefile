# Copyright 2018-2020 Minim Inc
# Wrapper makefile for iwinfo build

MODULE := iwinfo
VERSION ?= $(MODULE)-dev
REPO_URL := https://github.com/MinimSecure/iwinfo-lite
BUILD_PATH := $(TARGET_OBJ)/$(MODULE)
SOURCE_PATH := $(BUILD_PATH)/$(VERSION)
VERSION_REV := $(or $(patsubst iwinfo-lite-%,%,$(VERSION)) $(patsubst iwinfo-%,%,$(VERSION)))
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
	if [ ! -d "$(SOURCE_PATH)" ]; then \
		git clone --depth=1 $(REPO_URL) $(SOURCE_PATH); \
		git -C $(SOURCE_PATH) checkout $(VERSION_REV); \
	fi

.configured: $(SOURCE_PATH)
	touch $@

.compiled: .configured
	make -C $(SOURCE_PATH) BACKENDS="$(IWINFO_BACKENDS)" FPIC="-fPIC"
	touch $@ $^

.installed: .compiled
	mkdir -p $(TARGET_OBJ)/lib
	mkdir -p $(TARGET_OBJ)/include
	cp $(SOURCE_PATH)/libiwinfo.so $(TARGET_OBJ)/lib
	cp -r $(SOURCE_PATH)/include/* $(TARGET_OBJ)/include
	touch $@
