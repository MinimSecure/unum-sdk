// (c) 2019 minim.co
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
