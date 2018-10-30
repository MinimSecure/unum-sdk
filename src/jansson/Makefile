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

# Wrapper makefile for jansson library build

ifeq ($(VERSION),)
  $(error Please chose VERSION to build in the platform rule file)
endif

SUBDIR := $(VERSION)
MYSRCDIR := $(TOP)/src/jansson/$(SUBDIR)
CPPFLAGS += -I.

all: .compiled

$(SUBDIR):
	curl -L https://github.com/akheron/jansson/archive/$(word 2,$(subst -, ,$(VERSION))).tar.gz \
		> $(VERSION).tar.gz
	tar xf $(VERSION).tar.gz

$(SUBDIR)/configure:
	cd $(SUBDIR) && autoreconf -i

.compiled: $(wildcard $(MYSRCDIR)/**) .configured Makefile
	cd "$(SUBDIR)" && $(MAKE)
	touch $@

.configured: $(SUBDIR) Makefile $(SUBDIR)/configure $(SUBDIR)/configure.ac $(SUBDIR)/Makefile.in
	cd "$(SUBDIR)" && \
	./configure --build=x86_64-pc-linux-gnu --host=$(TARGET_PLATFORM_ARCH)
	touch $@
