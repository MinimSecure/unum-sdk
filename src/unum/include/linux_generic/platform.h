// Copyright 2018 Minim Inc
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Platform specific include file.
// The main purpose is to pull in all the platform specific headers.

#ifndef _PLATFORM_H
#define _PLATFORM_H

// Override the default config file path if ETC_PATH_PREFIX is defined
#ifdef ETC_PATH_PREFIX
#  undef UNUM_CONFIG_PATH
#  define UNUM_CONFIG_PATH ETC_PATH_PREFIX "/config.json"
#endif

#endif // _PLATFORM_H

