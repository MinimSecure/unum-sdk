// (c) 2017-2019 minim.co
// unum fetch_urls generic command handling

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

// URL to send the data from requested URL response, parameters:
// - the URL prefix
// - MAC addr of gateway (in xx:xx:xx:xx:xx:xx format)
// - "cookie" the server passed to the agent w/ the request info
#define FETCH_RSP_PATH "/v3/unums/%s/url_contents/%s"

// Defines for service IDs we use here
#define FU_HTTP_ID       "http://"
#define FU_HTTP_SZ       (sizeof(FU_HTTP_ID) - 1)
#define FU_HTTPS_ID      "https://"
#define FU_HTTPS_SZ      (sizeof(FU_HTTPS_ID) - 1)
#define FU_TELNET_ID     "telnet://"
#define FU_TELNET_SZ     (sizeof(FU_TELNET_ID) - 1)
#define FU_PINGFLOOD_ID  "pingflood://"
#define FU_PINGFLOOD_SZ  (sizeof(FU_PINGFLOOD_ID) - 1)


// Flag indicating the fetching URLs thread is running
static int fetch_urls_in_progress = FALSE;

// Fwetch URLs queue list head
static UTIL_QI_t head = { &head, &head };

// Fetch URL items array
static FETCH_URL_ITEM_t fetch_a[FETCH_URLS_QUEUE_LIMIT];

// Mutex for protecting fetch URLs globals and the queue
static UTIL_MUTEX_t fetch_m = UTIL_MUTEX_INITIALIZER;


// Send the fetched response data to the server
// Returns: 0 if successful, error code otherwise
static int report_rsp(char *cookie, char *data, int len)
{
    char url[256];
    http_rsp *rsp;
    char *my_mac = util_device_mac();

    // Check that we have MAC address
    if(!my_mac) {
        log("%s: cannot get device MAC\n", __func__);
        return -1;
    }

    // Post the downloaded data to the server
    util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API, url, sizeof(url),
                   FETCH_RSP_PATH, my_mac, cookie);

    rsp = http_put(url, "Accept: application/json\0", data, len);
    if(rsp == NULL) {
        return -2;
    }
    if((rsp->code / 100) != 2) {
        free_rsp(rsp);
        return -3;
    }
    free_rsp(rsp);
    return 0;
}

// Try to handle the server's request to download from URLs
// Returns: 0 if successful, error code otherwise
static int process_http_req(char *cookie, char *url, json_t *req)
{
    int do_get = TRUE;
    char *req_headers = NULL;
    char *req_data = "";
    int headers = FALSE;
    int headers_add = FALSE;
    int headers_only = FALSE;
    const char *tmp;
    json_t *val;
    http_rsp *rsp;
    int ret = 0;

    log("%s: processing http request [%s]\n", __func__, cookie);

    // Get the request type (optional)
    if((val = json_object_get(req, "method")) != NULL &&
       (tmp = json_string_value(val)) != NULL)
    {
        if(tmp && strcmp("GET", tmp) == 0) {
            do_get = TRUE;
        } else if(tmp && strcmp("POST", tmp) == 0) {
            do_get = FALSE;
        } else {
            log("%s: unrecognized method <%s>\n", __func__, tmp);
        }
    }

    // Get the request headers string (optional)
    if((val = json_object_get(req, "headers")) != NULL &&
       (tmp = json_string_value(val)) != NULL)
    {
        int ii, len = strlen(tmp);

        // Replace \n w/ 0, check that server added \n\n at the end
        // Note: we do not need space for the terminating 0 since the
        //       server sends us string w/ \n\n at the end
        req_headers = UTIL_MALLOC(len);
        for(ii = 0; ii < len; ii++) {
            req_headers[ii] = tmp[ii];
            if(tmp[ii] == '\n') {
                req_headers[ii] = 0;
            }
        }
        // Verify that we now have \0\0 at the end
        if(len < 2 || req_headers[len - 2] != 0 || req_headers[len - 1] != 0) {
            log("%s: malformed headers <%s>\n", __func__, tmp);
            UTIL_FREE(req_headers);
            req_headers = NULL;
        }
    }

    // Request body for POST (optional)
    if((val = json_object_get(req, "body")) != NULL &&
       (tmp = json_string_value(val)) != NULL)
    {
        req_data = (char *)tmp;
    }

    // Server asks for headers or the content (optional)
    if((val = json_object_get(req,"headers_only")) != NULL && json_is_true(val))
    {
        headers_only = headers = TRUE;
    }

    // Server asks for headers and body together in the content (optional)
    if((val = json_object_get(req,"headers_add")) != NULL && json_is_true(val))
    {
        headers_add = headers = TRUE;
    }

    // Set watchdog for 2 requests plus some extra time (it should be enough
    // time to pull from the requested URL and post the resut to the cloud
    util_wd_set_timeout(3 * HTTP_REQ_MAX_TIME);

    // Perform the request
    log("%s: %s request for %s to <%s>\n",
        __func__, (do_get ? "GET" : "POST"),
        (headers_only ? "headers" :
            (headers_add ? "headers and body" : "body")), url);

    if(do_get) {
        rsp = headers ?
              http_get_all(url, req_headers) :
              http_get(url, req_headers);
    } else {
        rsp = headers ?
              http_post_all(url, req_headers, req_data, strlen(req_data)) :
              http_post(url, req_headers, req_data, strlen(req_data));
    }

    // Cleanup request headers string (we no longer need it)
    if(req_headers) {
        UTIL_FREE(req_headers);
        req_headers = NULL;
    }

    // Report to the server the results we got back (if anything)
    if(rsp) {
        // If the server asked for headers only edit the response to
        // end at "\r\n\r\n"
        if(headers_only) {
            char *tmp = strstr(rsp->data, "\r\n\r\n");
            if(tmp) {
                *tmp = 0;
                rsp->len = tmp - rsp->data;
            }
        }
        if(report_rsp(cookie, rsp->data, rsp->len) != 0) {
            log("%s: failed to report response <%s>:<%s>\n",
                __func__, cookie, url);
            ret = -2;
        }
        free_rsp(rsp);
        rsp = NULL;
    } else {
        log("%s: no response from <%s>\n", __func__, url);
        ret = -3;
    }

    // Clear the watchdog
    util_wd_set_timeout(0);

    return ret;
}

// Try to handle the server's request to test telnet with username and password
// Returns: 0 if successful, error code otherwise
static int process_telnet_req(char *cookie, char *url, json_t *req)
{
    int retval = 0;
#ifdef FEATURE_TELNET_PASSWORDS
    TELNET_INFO_t info;
    char *past_schema = url + FU_TELNET_SZ;

    memset(&info, 0, sizeof(info));

    retval = split_telnet_uri(&info, past_schema, strlen(past_schema));
    if(retval < 0) {
        log("%s: failed to parse telnet uri:<%s>\n", __func__, url);
        return retval;
    }

    retval = attempt_telnet_login(&info);
    if(retval < 0) {
        log("%s: failed to process <%s>\n", __func__, url);
        return retval;
    }

    if(report_rsp(cookie, (char *)info.output, info.length) != 0) {
        log("%s: failed to report response <%s>:<%s>\n", __func__, cookie, url);
        retval = -2;
    }
#endif // FEATURE_TELNET_PASSWORDS
    return retval;
}

// Try to handle the server's request to test a device bandwidth
// using ICMP echo requeststs flooding
// Returns: 0 if successful, error code otherwise
static int process_pingflood_req(char *cookie, char *url, json_t *req)
{
    int retval = 0;
    char *target = url + FU_PINGFLOOD_SZ;

    char *jstr = do_pingflood_test(target);
    if(jstr == NULL) {
        log("%s: failed to process <%s>\n", __func__, url);
        return -1;
    }

    if(report_rsp(cookie, jstr, strlen(jstr)) != 0) {
        log("%s: failed to report response <%s>:<%s>\n", __func__, cookie, url);
        retval = -2;
    }

    util_free_json_str(jstr);

    return retval;
}

// Try to handle the server's request to connect to an URL
// Returns: 0 if successful, error code otherwise
static int process_req(char *cookie, json_t *req)
{
    char *url;
    json_t *val;

    log("%s: processing request [%s]\n", __func__, cookie);

    // Get the URL (required)
    if(!(val = json_object_get(req, "url")) ||
       !(url = (char *)json_string_value(val)))
    {
        log("%s: invalid/missing URL in the request\n", __func__);
        return -1;
    }

    if((strncmp(url, FU_HTTP_ID, FU_HTTP_SZ) == 0) ||
       (strncmp(url, FU_HTTPS_ID, FU_HTTPS_SZ) == 0))
    {
        return process_http_req(cookie, url, req);
    } else if(strncmp(url, FU_TELNET_ID, FU_TELNET_SZ) == 0) {
        return process_telnet_req(cookie, url, req);
    } else if(strncmp(url, FU_PINGFLOOD_ID, FU_PINGFLOOD_SZ) == 0) {
        return process_pingflood_req(cookie, url, req);
    }

    log("%s: unsupported URI scheme [%s]\n", __func__, url);
    return -1;
}

// Fetch URLs thread entry point
static void fetch_urls(THRD_PARAM_t *p)
{
    FETCH_URL_ITEM_t item, *p_item;

    for(;;)
    {
        // Pull item out from the fetch queue, make a local copy and free
        // the cell it was using
        UTIL_MUTEX_TAKE(&fetch_m);
        UTIL_Q_REM(&head, p_item);
        if(p_item == NULL) {
            // Done with all the queued items, can terminate the thread
            UTIL_MUTEX_GIVE(&fetch_m);
            break;
        }
        // Store the info in local data structure
        memcpy(&item, p_item, sizeof(item));
        // Free the cell
        p_item->root = p_item->req = NULL;
        UTIL_MUTEX_GIVE(&fetch_m);

        // Make the request
        process_req(item.cookie, item.req);

        // Decrement the reference count
        json_decref(item.root);
    }

    fetch_urls_in_progress = FALSE;
}

// Add request to the fetch queue and increment root JSON refcount.
// Note: this function call should be protected by "fetch_m" mutex!
// cookie - the cookie string
// root - root JSON object containing cookie and req content
// req - JSON object containing info on what request the server wants the
//       agent to make
// Returns: 0 if ok, negative error code if fails, positive if duplicate
static int add_req(const char *cookie, json_t *root, json_t *req)
{
    int ret;

    // If parameters are not valid back off immediately
    if(!root || !req || !cookie) {
        return -1;
    }

    for(;;)
    {
        FETCH_URL_ITEM_t *item;

        // Make sure we do not have duplicates in to-do list
        UTIL_Q_FOREACH(&head, item) {
            if(strncmp(item->cookie, cookie, FETCH_URLS_MAX_COOKIE_LEN) == 0)
            {
                ret = 1;
                break;
            }
        }

        // Pick a cell (start at random spot)
        int idx, ii;
        idx = rand() % FETCH_URLS_QUEUE_LIMIT;
        for(ii = 0; ii < FETCH_URLS_QUEUE_LIMIT; ii++) {
            if(!fetch_a[idx].req) {
                break;
            }
            ++idx;
            idx %= FETCH_URLS_QUEUE_LIMIT;
        }
        if(fetch_a[idx].req) {
            log("%s: error, no free cell for request <%s>\n",
                __func__, cookie);
            ret = -2;
            break;
        }

        // Start the fetch URLs thread if it is not running, we are
        // under mutex, so it will wait till we populate the queue
        ret = 0;
        if(!fetch_urls_in_progress) {
            fetch_urls_in_progress = TRUE;
            if(util_start_thrd("fetch_urls", fetch_urls, NULL, NULL) != 0) {
                ret = -3;
                break;
            }
        }

        // Set up the fetch structure and increment refcount for the request
        // NOTE: the refcount is incremented for each queued request here and
        //       decremented in the fetch thread for each request processed
        fetch_a[idx].root = json_incref(root);
        fetch_a[idx].req = req;
        strncpy(fetch_a[idx].cookie, cookie, FETCH_URLS_MAX_COOKIE_LEN);
        fetch_a[idx].cookie[FETCH_URLS_MAX_COOKIE_LEN] = 0;

        // Add the item to the end of the linked list
        UTIL_Q_ADD(&head, &(fetch_a[idx]));

        ret = 0;
        break;
    }

    return ret;
}

// Generic processing function for fetching URLs and URL headers
// command ("fetch_urls")
// The parameters: command name, JSON object w/ "cookie":{...}
// key/value pairs, JSON content len
// The content is 0-terminated, but 0 might be beyond len.
// Returns: 0 if successfully processed all the requests (even if
//          unable to retrieve content)
int cmd_fetch_urls(char *cmd, char *s, int s_len)
{
    json_t *rsp_root = NULL;
    json_error_t jerr;
    int err = -1;

    for(;;)
    {
        const char *cookie;
        json_t *req;
        int req_count;

        if(!s || s_len <= 0) {
            log("%s: error, data: %s, len: %d\n", __func__, s, s_len);
            break;
        }

        // Process the incoming JSON. It should contain request list object
        // if the form { "cookie1" : { ... }, "cookie2" : { ... }, ...}
        rsp_root = json_loads(s, JSON_REJECT_DUPLICATES, &jerr);
        if(!rsp_root) {
            log("%s: error at l:%d c:%d parsing request list: '%s'\n",
                __func__, jerr.line, jerr.column, jerr.text);
            break;
        }

        // Make sure we have an object and something inside
        req_count = json_object_size(rsp_root);
        if(req_count <= 0) {
            log("%s: malformed or empty request list object (items: %d)\n",
                __func__, req_count);
            break;
        }

        // Loop through the key/value pairs. The whole loop is protected by
        // mutex since we want to queue all the items before processing them
        // (otherwise we might try to free a part of this json object while
        //  still in the loop)
        UTIL_MUTEX_TAKE(&fetch_m);
        json_object_foreach(rsp_root, cookie, req)
        {
            if(add_req(cookie, rsp_root, req) != 0) {
                log("%s: failed to enqueue request w/ cookie <%s>\n",
                    __func__, cookie);
            }
        }
        UTIL_MUTEX_GIVE(&fetch_m);

        err = 0;
        break;
    }

    if(rsp_root) {
        json_decref(rsp_root);
        rsp_root = NULL;
    }

    return err;
}
