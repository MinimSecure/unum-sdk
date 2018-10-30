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

# Wrapper makefile for mbedtls libraries build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/mbedtls/$(SUBDIR)
CFLAGS += -I. $(CPPFLAGS)

all: .compiled

$(SUBDIR):
	curl -L https://tls.mbed.org/download/mbedtls-2.4.2-apache.tgz > $(SUBDIR).tgz
	tar -xf $(SUBDIR).tgz

.compiled: Makefile $(SUBDIR) | $(wildcard $(MYSRCDIR)/**)
	cd "$(SUBDIR)" && $(MAKE) SHARED=1 no_test
	# Make lib folder with libs for dependent packages to refer to
	cd "$(SUBDIR)" && mkdir -p ./lib
	cd "$(SUBDIR)/lib" && cp -fP ../library/*.so ../library/*.so.* ./
	touch $@
