// (c) 2017-2018 minim.co
// curl specific include
// Any data type, constant needed in the common header, but
// having specific value for curl goes here.
// This file should also pull all the curl headers needed.
// Note: it is shared between all platforms using curl,
//       therefore if something is specific for the platform,
//       but not curl it goes to the http.h in the device
//       subfolder

#ifndef _HTTP_CURL_H
#define _HTTP_CURL_H

// headers required for curl
#include <curl/curl.h>
#include <resolv.h>

// How many times to try performing the request till get the
// response (error responses do not cause retries).
#define REQ_RETRIES 3

// timeouts for curl
#define REQ_CONNECT_TIMEOUT    20
#define REQ_API_TIMEOUT        30
#define REQ_API_TIMEOUT_SHORT  5
#define REQ_FILE_TIMEOUT       1200

// Timeouts used in common code for setting up watchdog
// MAX time for HTTP REST API operations
#define HTTP_REQ_MAX_TIME ((REQ_CONNECT_TIMEOUT + REQ_API_TIMEOUT) * REQ_RETRIES)
// MAX time for HTTP file upload/download operations
#define HTTP_FILE_MAX_TIME ((REQ_CONNECT_TIMEOUT + REQ_FILE_TIMEOUT) * REQ_RETRIES)

#endif // _HTTP_CURL_H
