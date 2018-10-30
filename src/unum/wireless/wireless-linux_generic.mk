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

# Makefile for linux_generic ieee802.11 functionality

# Add model specific include path to make wireless.h
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/wireless/$(MODEL)

# Add code file(s)
OBJECTS += ./wireless/$(MODEL)/radios_platform.o \
           ./wireless/$(MODEL)/stas_platform.o   \
           ./wireless/$(MODEL)/scan_platform.o   \
           ./wireless/wireless_iwinfo.o          \
           ./wireless/$(MODEL)/wireless_platform.o

# Include common portion of the makefile for the subsystem
include ./wireless/wireless_common.mk
