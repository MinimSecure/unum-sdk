# Copyright 2020 Minim Inc
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

# Device fingerprinting information collection and reporting

# Add platform folder to the include search path
CPPFLAGS += -I$(UNUM_PATH)/fingerprint/$(MODEL)

# Add code file(s)
OBJECTS += \
  ./fingerprint/fp_main.o \
  ./fingerprint/fp_ssdp.o \
  ./fingerprint/fp_dhcp.o \
  ./fingerprint/fp_mdns.o \
  ./fingerprint/fp_useragent.o

# Add subsystem initializer function
INITLIST += fp_init
