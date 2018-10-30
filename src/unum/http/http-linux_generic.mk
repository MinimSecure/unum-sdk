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

# Plaform independet Makefile for curl-based http functions library 

# Add model specific include path to make http.h 
# come from the right place
CPPFLAGS += -I$(UNUM_PATH)/http/$(MODEL)

# Add code file(s)
OBJECTS += ./http/http_curl.o

# Add subsystem initializer function
INITLIST += http_init
