# Copyright 2018 - 2020 Minim Inc
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

# Platform makefile for unum agent executable build

# Add linking flags for libs, they should be available
# since our package depends on those libs

HARDWARE = linux_generic
AP_HARDWARE_TYPES =

# Generic libs
LDFLAGS += -lrt -lcrypto
CPPFLAGS += -DUSE_OPEN_SSL -D_DEFAULT_SOURCE

# Add linking flags for libs that we built before the agent
# (those migh differ for the platforms, so include conditionally
# in case we need to share these additions a lot).
ifneq ($(CURL),)
  CPPFLAGS += -I$(TARGET_OBJ)/curl/$(CURL)/include
  LDFLAGS += -L$(TARGET_OBJ)/curl/$(CURL)/lib/.libs -l:libcurl.so
  #LDFLAGS += -L$(TARGET_OBJ)/curl/$(CURL)/lib/.libs -l:libcurl.a
else
  LDFLAGS += -lcurl
endif
ifneq ($(JANSSON),)
  CPPFLAGS += -I$(TARGET_OBJ)/jansson/$(JANSSON)/src
  LDFLAGS += -L$(TARGET_OBJ)/jansson/$(JANSSON)/src/.libs -l:libjansson.so
else
  LDFLAGS += -ljansson
endif
ifneq ($(IWINFO),)
  CPPFLAGS += -I$(TARGET_OBJ)/iwinfo/$(IWINFO)
  LDFLAGS += -L$(TARGET_OBJ)/iwinfo/$(IWINFO) -liwinfo
endif

LDFLAGS += -L$(TARGET_LIBS)/

# Add hardware ID this build is for
CPPFLAGS += -DDEVICE_PRODUCT_NAME=\"$(HARDWARE)\"
CPPFLAGS += -Wno-format
CPPFLAGS += -Wno-int-to-pointer-cast
