# (c) 2017-2019 minim.co
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
OBJECTS := ./unum.o ./agent.o ./monitor.o ./tracer.o ./stubs.o
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

# Add feature list defines. Not all of them are important for the agent code.
# Here are some that are used (there are more, not all are listed here):
# FEATURE_LAN_ONLY - the platform is a standalone AP-only or managed device,
#                    unlike the AP mode for gateway devices this option turns on
#                    collecting devices telemetry for traffic crossing the LAN
#                    bridge (i.e. from wireless to Ethernet or other wireless
#                    interface). For managed devices the agent watches its
#                    own traffic only (i.e. to/from local interfaces)
# FEATURE_AP_MODE -  the platform supports operating in the AP mode (in this
#                    mode the agent is expected to run on a device operating as
#                    an AP or extender in a LAN alredy running managed gateway,
#                    when in this mode the agent does not collect telemetry
#                    duplicating that of the LAN gateway).
# FEATURE_MESH_11S - the platform supports IEEE 802.11s mesh and has to handle
#                    meash_11s_ap/meash_11s_gw operation modes, it might also
#                    do some extra work to collect info about the 802.11s mesh
#                    interfaces and STAs.
CPPFLAGS += $(RELEASE_DEFINES)

# Default goal
all: unum

unum: $(OBJECTS) $(XOBJECTS)
	$(GCC) $(CFLAGS) -o unum $^ $(LDFLAGS)

$(OBJECTS): %.o: %.c
	$(GCC) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

$(XOBJECTS): %.o: %.c
	$(GCC) -c $(CPPFLAGS) $(CFLAGS) -std=c99 -o $@ $<


# Do not really need to generate .d for each file since all sources
# pull all headers in through single unum.h include.
DEPENDS := $(OBJECTS:%.o=%.d)
$(DEPENDS): %.d: %.c Makefile
	echo -n "$@ $(dir $<)" > $@
	$(GCC) -M $(CPPFLAGS) $(CFLAGS) $< | grep -v '^#' >> $@ || rm -f "$@"

-include $(DEPENDS)
