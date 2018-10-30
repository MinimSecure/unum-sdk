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

// HTTP APIs platform specific include file

#ifndef _HTTP_H
#define _HTTP_H

// The platform uses libcurl to do HTTP, the below include
// pulls in all the CURL headers needed (included by all platforms
// that use curl)
#include "../http_curl.h"

// Common header describing APIs shared across all platforms
// and HTTP libraries.
#include "../http_common.h"

// Anything specific for this particular platform should
// stay in this file.

#endif // _HTTP_H
