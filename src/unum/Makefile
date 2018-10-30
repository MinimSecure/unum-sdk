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

# Makefile for unum agent executable build

# Helper variables
UNUM_PATH := $(TARGET_OBJ)/unum

# Add generic include paths
CPPFLAGS += -I$(UNUM_PATH)/include/$(MODEL) -I$(UNUM_PATH)/include

# Add flags for debug build
ifneq ($(UNUM_DEBUG),)
  CPPFLAGS += -DDEBUG
  CFLAGS += -O0
endif

# Add define for flagging developer builds
ifeq ($(UNUM_RELEASE_BUILD),)
  CPPFLAGS += -DDEVELOPER_BUILD
endif

# Pull in all subfolder makefiles for the platform/device model.
# The variables populated by those makefiles:
# OBJECTS - list of .o files to build (with path from unum folder)
# INITLIST - list of init functions unum has to execute
# CPPFLAGS,CFLAGS,CXXFLAGS - extra c,c++ flags the subfolder code might need
# LDFLAGS - extra linking flags the subfolder code might need

CPPFLAGS += -pthread -Wall -Werror -I$(UNUM_PATH)
LDFLAGS += -pthread
OBJECTS := ./unum.o ./agent.o ./monitor.o ./tracer.o
INITLIST := agent_init

-include ./unum-$(MODEL).mk
-include $(wildcard ./*/*-$(MODEL).mk)

# Build comma-separated lists of functions and their names from INITLIST
quote := "
space := $(null) #
comma := ,
INITFLIST := $(subst $(space),$(comma),$(strip $(INITLIST)))
INITSLIST := $(subst $(space),$(comma),$(patsubst %,$(quote)%$(quote),$(INITLIST)))

# Add comma-separated init list and unum version defines
CPPFLAGS += -DVERSION=\"$(AGENT_VERSION)\" -D'INITLIST=$(INITFLIST)' -D'INITSTRLIST=$(INITSLIST)'

# Add model type define
CPPFLAGS += -DMODEL_$(MODEL)

# Enable crash handler/tracer
ifeq ($(UNUM_DISABLE_TRACER),)
  CPPFLAGS += -DAGENT_TRACE
endif

# Add define to comple the agent for AP-only mode of operation
ifneq ($(UNUM_AP_MODE),)
  CPPFLAGS += -DAP_MODE
endif

# Add define to enable hardware acceleration support
# Note: this requires specific platform support.
ifneq ($(UNUM_HARDWARE_ACCEL),)
  CPPFLAGS += -DHARDWARE_ACCEL
endif

# Default goal
all: unum

unum: $(OBJECTS)
	$(GCC) $(CFLAGS) -o unum $^ $(LDFLAGS)

$(OBJECTS): %.o: %.c
	$(GCC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

# Do not really need to generate .d for each file since all sources
# pull all headers in through single unum.h include.
DEPENDS := $(OBJECTS:%.o=%.d)
$(DEPENDS): %.d: %.c Makefile
	echo -n "$@ $(dir $<)" > $@
	$(GCC) -M $(CPPFLAGS) $(CFLAGS) $< >> $@ || rm -f "$@"

-include $(DEPENDS)
