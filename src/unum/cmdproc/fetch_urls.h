// (c) 2017-2018 minim.co
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
