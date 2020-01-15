#!/bin/sh
# (c) 2019 - 2020 minim.co
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

# Apply settings like timezone, hostname, logs etc
/etc/init.d/system restart
# Network settings
/etc/init.d/network restart
# This seems to take some time even after command completion.
# Will remove sleep if not needed
sleep 5
#DHCP and DNS server settings
/etc/init.d/dnsmasq restart
# Firewall settings
/etc/init.d/firewall restart
