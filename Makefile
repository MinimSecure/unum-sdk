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

#
# Makefile for compiling all components required for unum 
# integration into specific platform.
# The platform is specified by setting the MODEL variable.
# The ./rules/$(MODEL).mk file determines the list of components to 
# to build and the build flags/options.

# Figure the top level directory
TOP := $(patsubst %/,%,$(dir $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

# Disable implicit rules 
ifeq ($(filter -r,$(MAKEFLAGS)),)
  MAKEFLAGS += -r
endif

# Default platform to build for (unless specified)
MODEL ?= linux_generic

# Generate package version # (unless specified)
ifeq ($(strip $(AGENT_VERSION)),)
  AGENT_VERSION := $(shell date +%Y.%m%d.%H%M%S.0)
else
  AGENT_VERSION := $(strip $(AGENT_VERSION))
endif

TARGET_SRC := $(TOP)/src/$(MODEL)
TARGET_FILES := $(TOP)/files/$(MODEL)

TARGET_OUT := $(TOP)/out/$(MODEL)

TARGET_BUILD := $(TOP)/build/$(MODEL)
TARGET_RFS := $(TARGET_BUILD)/rfs
TARGET_OBJ := $(TARGET_BUILD)/obj

# Explicitly set default goal
all: install

# Include the model makefile and prepare global vars
include $(TOP)/rules/$(MODEL).mk
export

# Dependencies for specific software components
ALL_SOURCE_FOLDERS := $(patsubst %,$(TOP)/src/%,$(TARGET_LIST))
ALL_OBJ_FOLDERS := $(patsubst %,$(TARGET_OBJ)/%,$(TARGET_LIST))
ALL_INCLUDES  := $(patsubst %,$(TARGET_OBJ)/.%.include,$(TARGET_LIST))
ALL_PREPARED  := $(patsubst %,$(TARGET_OBJ)/.%.prepared,$(TARGET_LIST))
ALL_COMPILED  := $(patsubst %,$(TARGET_OBJ)/.%.compiled,$(TARGET_LIST))
ALL_INSTALLED := $(patsubst %,$(TARGET_OBJ)/.%.installed,$(TARGET_LIST))
ALL_CLEAN_RULES := $(patsubst %,%.clean,$(TARGET_LIST)) 
ALL_INSTALL_RULES := $(patsubst %,%.install,$(TARGET_INSTALL_LIST)) 

.PHONY: install build clean install_prepare make_includes
.PHONY: $(TARGET_LIST) $(ALL_CLEAN_RULES) $(ALL_INSTALL_RULES)

# High level dependencies
$(TARGET_OUT):
	mkdir -p $(TARGET_OUT)

$(TARGET_RFS):
	mkdir -p $(TARGET_RFS)

$(TARGET_OBJ):
	mkdir -p $(TARGET_OBJ)

$(TARGET_LIST): %: $(TARGET_OBJ)/.%.compiled

$(ALL_COMPILED): $(TARGET_OBJ)/.%.compiled: $(TARGET_OBJ)/.%.prepared
	echo "Compiling: $*"
	export LD_LIBRARY_PATH=$(LD_PATH_PREFIX):$$LD_LIBRARY_PATH PATH=$(PATH_PREFIX):$$PATH \
               CPPFLAGS="$(TARGET_FLAGS) $(TARGET_CPPFLAGS) $(TARGET_CPPFLAGS_$*)" \
               CFLAGS="$(TARGET_CFLAGS) $(TARGET_CFLAGS_$*)" \
               LDFLAGS="$(TARGET_FLAGS) $(TARGET_LDFLAGS) $(TARGET_LDFLAGS_$*)" \
               $(TARGET_VARS_$*) \
               MAKEFLAGS="$(TARGET_MAKEFLAGS)" \
               && $(MAKE) -C "$(TARGET_OBJ)/$*"
	touch $@

$(ALL_PREPARED): $(TARGET_OBJ)/.%.prepared: $(TOP)/Makefile $(TOP)/rules/$(MODEL).mk | $(TARGET_OBJ) $(TARGET_RFS) $(TARGET_OUT)
	echo "Preparing: $*"
	mkdir -p "$(TARGET_OBJ)/$*"
	cd "$(TARGET_OBJ)/$*" && find -xtype l -exec rm -f {} \;
	cd "$(TOP)/src/$*" && find * -type d -exec mkdir -p "$(TARGET_OBJ)/$*/{}" \;
	cd "$(TOP)/src/$*" && find * -type f -exec ln -sf "$(TOP)/src/$*/{}" "$(TARGET_OBJ)/$*/{}" \;
	cd "$(TOP)/src/$*" && find * -type l -exec cp -avf "$(TOP)/src/$*/{}" "$(TARGET_OBJ)/$*/{}" \;
	touch $@

$(ALL_INCLUDES): $(TARGET_OBJ)/.%.include: $(wildcard $(TOP)/src/**) $(TARGET_OBJ)
	echo -n "$(TARGET_OBJ)/.$*.prepared : " > "$(TARGET_OBJ)/.$*.include"
	find $(TOP)/src/$* $(TOP)/libs/$(MODEL) -type f -printf '%p ' >> "$(TARGET_OBJ)/.$*.include"

include $(ALL_INCLUDES)

build: $(TARGET_LIST)

install_prepare: build
	echo "Populating root filesystem tree for $(MODEL)"
	-rm -Rf "$(TARGET_RFS)"

$(ALL_INSTALL_RULES): install_prepare

install: $(ALL_INSTALL_RULES)
	-rm -Rf $(TARGET_OUT)/*.*
	tar -C $(TARGET_RFS) -czvf $(TARGET_OUT)/$(MODEL)-$(AGENT_VERSION).tgz .
	echo "Unum agent package:"
	@ls -l $(TARGET_OUT)/$(MODEL)-$(AGENT_VERSION).tgz

$(ALL_CLEAN_RULES): %.clean:
	-rm -Rf $(TARGET_OBJ)/$*
	-rm -f $(TARGET_OBJ)/.$*.*

clean:
	-rm -Rf $(TARGET_OUT)
	-rm -Rf $(TARGET_RFS)
	-rm -Rf $(TARGET_OBJ)
