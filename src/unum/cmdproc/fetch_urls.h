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

// unum command processor fetch URLs include file

#ifndef _FETCH_URLS_H
#define _FETCH_URLS_H

// URLs to download queue limit
#define FETCH_URLS_QUEUE_LIMIT 64

// MAX allowed length of the cookie
#define FETCH_URLS_MAX_COOKIE_LEN 32


// Download URL queue item data structure
typedef struct _FETCH_URL_ITEM {
    UTIL_Q_HEADER(); // declares fields used by queue macros
    char cookie[FETCH_URLS_MAX_COOKIE_LEN + 1]; // cookie string
    json_t *req;     // json w/ request parameters
    json_t *root;    // root JSON object (used to track reference count)
} FETCH_URL_ITEM_t;


// Generic processing function for fetching URLs and URL headers
// command ("fetch_urls")
// The parameters: command name, JSON object w/ "cookie":{...}
// key/value pairs, JSON content len
// The content is 0-terminated, but 0 might be beyond len.
// Returns: 0 if successfully processed all the requests (even if
//          unable to retrieve content)
int cmd_fetch_urls(char *cmd, char *s, int s_len);

#endif // _FETCH_URLS_H
