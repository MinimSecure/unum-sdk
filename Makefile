# (c) 2017-2020 minim.co
#
# Makefile for compiling all components required for unum 
# integration into specific platform.
# The platform is specified by setting the MODEL variable.
# The ./rules/$(MODEL).mk file determines the list of components
# to build and the build flags/options.
# All the model-specific files can be kept in a separate repository
# that mirrors the directory structure of this one. When building
# the full path to the external repo should be passed in in the
# ADD_TARGET variable.

# Figure the top level directory
TOP := $(patsubst %/,%,$(dir $(CURDIR)/$(word $(words $(MAKEFILE_LIST)),$(MAKEFILE_LIST))))

# The path to the external code to pull in MODEL-specific files
# during the build
X_TOP := $(ADD_TARGET)

# Disable implicit rules 
ifeq ($(filter -r,$(MAKEFLAGS)),)
  MAKEFLAGS += -r
endif

# Choose the platform to build for (unless specified)
ifeq ($(strip $(MODEL)),)
  MODEL := $(shell cat ~/unum-v2-default-model)
else
  MODEL := $(strip $(MODEL))
endif
ifeq ($(strip $(MODEL)),)
  $(error MODEL is required!)
endif

# Generate package version # (unless specified)
ifeq ($(strip $(AGENT_VERSION)),)
  AGENT_VERSION := $(shell date +%Y.%m%d.%H%M%S.0)
else
  AGENT_VERSION := $(strip $(AGENT_VERSION))
endif

# Where common files are located
COMMON_TOP := $(TOP)
COMMON_FILES := $(COMMON_TOP)/files

# If external MODEL-specifc location is specified pull the
# taget files from there instead of the common local tree
ifeq ($(strip $(X_TOP)),)
  TARGET_TOP := $(TOP)
else
  TARGET_TOP := $(X_TOP)
endif
TARGET_FILES := $(TARGET_TOP)/files/$(MODEL)
TARGET_RULES := $(TARGET_TOP)/rules/$(MODEL).mk
TARGET_LIBS := $(TARGET_TOP)/libs/$(MODEL)

# Where to copy build results
TARGET_OUT := $(TOP)/out/$(MODEL)

# Where to build
TARGET_BUILD := $(TOP)/build/$(MODEL)
TARGET_RFS := $(TARGET_BUILD)/rfs
TARGET_OBJ := $(TARGET_BUILD)/obj

# Add common and platform specific features to the release feature list
RELEASE_FEATURES := $(shell cat "$(TOP)/files/features-common.txt")
ifneq ("$(wildcard $(TARGET_FILES)/features.txt)","")
  RELEASE_FEATURES := $(RELEASE_FEATURES) $(shell cat "$(TARGET_FILES)/features.txt")
endif

# Explicitly set default goal
all: install

# Include the model makefile and prepare global vars
include $(TARGET_RULES)
export

# Dependencies for specific software components
ALL_SOURCE_FOLDERS := $(patsubst %,$(TOP)/src/%,$(TARGET_LIST))
ifneq ($(strip $(X_TOP)),)
  ALL_SOURCE_FOLDERS += $(patsubst %,$(X_TOP)/src/%,$(TARGET_LIST))
endif
ALL_OBJ_FOLDERS := $(patsubst %,$(TARGET_OBJ)/%,$(TARGET_LIST))
ALL_INCLUDES  := $(patsubst %,$(TARGET_OBJ)/.%.include,$(TARGET_LIST))
ALL_PREPARED  := $(patsubst %,$(TARGET_OBJ)/.%.prepared,$(TARGET_LIST))
ALL_COMPILED  := $(patsubst %,$(TARGET_OBJ)/.%.compiled,$(TARGET_LIST))
ALL_INSTALLED := $(patsubst %,$(TARGET_OBJ)/.%.installed,$(TARGET_LIST))
ALL_CLEAN_RULES := $(patsubst %,%.clean,$(TARGET_LIST)) 
ALL_INSTALL_RULES := $(patsubst %,%.install,$(TARGET_INSTALL_LIST)) 

# Create comma separated quoted release features list prepeared
# for inserting into release_properties.json and release features
# defines (-DFEATURE_<name_w_underscores_instead_of_dashes>)
quote := "
space := $(null) #
comma := ,
dash  := -
under := _
uc = $(subst a,A,$(subst b,B,$(subst c,C,$(subst d,D,$(subst e,E,$(subst f,F,$(subst \
             g,G,$(subst h,H,$(subst i,I,$(subst j,J,$(subst k,K,$(subst l,L,$(subst \
             m,M,$(subst n,N,$(subst o,O,$(subst p,P,$(subst q,Q,$(subst r,R,$(subst \
             s,S,$(subst t,T,$(subst u,U,$(subst v,V,$(subst w,W,$(subst x,X,$(subst \
             y,Y,$(subst z,Z,$1))))))))))))))))))))))))))
RELEASE_FEATURES := $(sort $(strip $(RELEASE_FEATURES)))
RELEASE_DEFINES := $(patsubst %,-DFEATURE_%,$(subst $(dash),$(under),$(call uc,$(RELEASE_FEATURES))))
RELEASE_FEATURES := $(subst $(space),$(comma),$(patsubst %,$(quote)%$(quote),$(RELEASE_FEATURES)))

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

$(ALL_PREPARED): $(TARGET_OBJ)/.%.prepared: $(TOP)/Makefile $(TARGET_RULES) | $(TARGET_OBJ) $(TARGET_RFS) $(TARGET_OUT)
	echo "Preparing: $*"
	mkdir -p "$(TARGET_OBJ)/$*"
	cd "$(TARGET_OBJ)/$*" && find -xtype l -exec rm -f {} \;
	# common files
	if [ -d "$(TOP)/src/$*" ]; then \
          cd "$(TOP)/src/$*";\
          find * -type d -exec mkdir -p "$(TARGET_OBJ)/$*/{}" \; ;\
          find * -type f -exec ln -sf "$(TOP)/src/$*/{}" "$(TARGET_OBJ)/$*/{}" \; ;\
          find * -type l -exec cp -avf "$(TOP)/src/$*/{}" "$(TARGET_OBJ)/$*/{}" \; ;\
        fi
ifneq ($(strip $(X_TOP)),)
	# external files (can override common files, folders and symlinks)
	if [ -d "$(X_TOP)/src/$*" ]; then \
          cd "$(X_TOP)/src/$*";\
          find * -type d -exec mkdir -p "$(TARGET_OBJ)/$*/{}" \; ;\
          find * -type f -exec rm -f "$(TARGET_OBJ)/$*/{}" \; -exec ln -sf "$(X_TOP)/src/$*/{}" "$(TARGET_OBJ)/$*/{}" \; ;\
          find * -type l -exec rm -rf "$(TARGET_OBJ)/$*/{}" \; -exec cp -avf "$(X_TOP)/src/$*/{}" "$(TARGET_OBJ)/$*/{}" \; ;\
        fi
endif
	touch $@

$(ALL_INCLUDES): $(TARGET_OBJ)/.%.include: $(wildcard $(TOP)/src/**) $(if $(X_TOP),$(wildcard $(X_TOP)/src/**)) $(TARGET_OBJ)
	echo -n "$(TARGET_OBJ)/.$*.prepared : " > "$(TARGET_OBJ)/.$*.include"
	if [ -d "$(TOP)/src/$*" ]; then find "$(TOP)/src/$*" -type f -printf '%p ' >> "$(TARGET_OBJ)/.$*.include"; fi
	if [ -d "$(TOP)/libs/$(MODEL)" ]; then find "$(TOP)/libs/$(MODEL)" -type f -printf '%p ' >> "$(TARGET_OBJ)/.$*.include"; fi
ifneq ($(strip $(X_TOP)),)
	if [ -d "$(X_TOP)/src/$*" ]; then find "$(X_TOP)/src/$*" -type f -printf '%p ' >> "$(TARGET_OBJ)/.$*.include"; fi
	if [ -d "$(X_TOP)/libs/$(MODEL)" ]; then find "$(X_TOP)/libs/$(MODEL)" -type f -printf '%p ' >> "$(TARGET_OBJ)/.$*.include"; fi
endif

# pull in all .include files with dependencies (except when cleaning)
ifneq ($(MAKECMDGOALS),clean)
  include $(ALL_INCLUDES)
endif

build: $(TARGET_LIST)

install_prepare: build
	echo "Populating root filesystem tree for $(MODEL)"
	-rm -Rf "$(TARGET_RFS)"

$(ALL_INSTALL_RULES): install_prepare

install: $(ALL_INSTALL_RULES)
	-rm -Rf $(TARGET_OUT)/*.*
	tar -C $(TARGET_RFS) --numeric-owner --group=0 --owner=0 -czvf $(TARGET_OUT)/$(MODEL)-$(AGENT_VERSION).tgz .
	@echo "Generating $(TARGET_OUT)/release_properties.json"
	envsubst < $(COMMON_FILES)/release_properties.json > $(TARGET_OUT)/release_properties.json
	echo "Unum agent package:"
	@ls -l $(TARGET_OUT)/$(MODEL)-$(AGENT_VERSION).tgz

$(ALL_CLEAN_RULES): %.clean:
	-rm -Rf $(TARGET_OBJ)/$*
	-rm -f $(TARGET_OBJ)/.$*.*

clean:
	-rm -Rf $(TARGET_OUT)
	-rm -Rf $(TARGET_RFS)
	-rm -Rf $(TARGET_OBJ)
