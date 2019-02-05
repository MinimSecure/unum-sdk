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

# Stub rule file for Unum for OpenWrt.


# This file is not used while building Unum for OpenWrt!
# Instead, use the OpenWrt feed published at:
#    https://github.com/MinimSecure/minim-openwrt-feed

# Check README-openwrt_generic.md in this repository for more information.


define ERRMSG
unable to continue
Unum for OpenWrt must be built using an OpenWrt toolchain.
Check README-openwrt_generic.md in this repository for instructions
Or see:
  https://github.com/MinimSecure/unum-sdk/blob/master/README-openwrt_generic.md
endef

__err := $(error $(ERRMSG))
