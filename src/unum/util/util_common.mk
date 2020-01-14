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

# Plaform independent part of Makefile for unum utility functions library 

# Add code file(s)
OBJECTS += ./util/util.o ./util/jobs.o ./util/util_event.o ./util/util_net.o
OBJECTS += ./util/util_json.o ./util/util_timer.o ./util/util_crashinfo.o
OBJECTS += ./util/$(MODEL)/util_platform.o ./util/util_stubs.o ./util/util_dns.o

# Add C99 Code Files
XOBJECTS = ./util/dns.o

# Add subsystem initializer function
INITLIST += util_init

