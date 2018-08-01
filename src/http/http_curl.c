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

// HTTP APIs implementation on top of libcurl

#include "unum.h"

// All logging from this file goes to HTTP log
#undef LOG_DST
#undef LOG_DBG_DST
#define LOG_DST LOG_DST_HTTP
#define LOG_DBG_DST LOG_DST_HTTP
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE

// Cut off HTTP data in the log msgs if too long
#define MAX_LOG_DATA_LEN 1024

// Static host:port to IPs mapping to use if DNS is not working.
// This list of pointers to the mapings should be terminated w/ NULL.
// The mapings should be in the form: HOST:PORT:ADDRESS[,ADDRESS]...
static char *dns_map[] = {
    "api.minim.co:80:34.207.26.129",
    "api.minim.co:443:34.207.26.129",
    "my.minim.co:443:34.207.26.129",
    "releases.minim.co:443:34.207.26.129",
    "provision.minim.co:443:34.207.26.129",
    NULL
};

// Returns NULL-teminated array of strings w/ hardcoded DNS names mapping
// http subsystem will use if DNS is not working. The entries are in
// the HOST:PORT:ADDRESS[,ADDRESS]... format.
char **http_dns_name_list(void)
{
    return dns_map;
}

// Download file from a URL
// The headers are passed as double 0 terminated multi-string.
// Returns 0 if successful error otherwise
int http_get_file(char *url, char *headers, char *file)
{
    CURL *ch;
    int err = -1;
    int retry;
    FILE *f;
    char *hdr;
    struct curl_slist *slhdr = NULL;
    struct curl_slist *sldns = NULL;
    char err_buf[CURL_ERROR_SIZE] = "";

    log("%s: file <%s>, url <%s>\n", __func__, file, url);

    f = fopen(file, "wb");
    if(!f) {
        log("%s: failed to open '%s', error (%s)\n",
            __func__, file, strerror(errno));
        return err;
    }

    if(headers)
    {
        struct curl_slist *slold = NULL;
        for(hdr = headers; *hdr != 0; hdr += strlen(hdr) + 1)
        {
            slold = slhdr;
            slhdr = curl_slist_append(slhdr, hdr);
            if(!slhdr) {
                break;
            }
        }
        if(!slhdr) {
            log("%s: failed to add headers, ignoring\n", __func__);
            if(slold) {
                curl_slist_free_all(slold);
            }
        }
    }

    if(conncheck_no_dns())
    {
        int ii;
        struct curl_slist *slold = NULL;
        for(ii = 0; dns_map[ii] != NULL; ++ii)
        {
            slold = sldns;
            sldns = curl_slist_append(sldns, dns_map[ii]);
            if(!sldns) {
                break;
            }
        }
        if(ii > 0 && !sldns) {
            log("%s: failed to add static DNS entries, ignoring\n", __func__);
            if(slold) {
                curl_slist_free_all(slold);
            }
        }
    }

    for(retry = 0; err != 0 && retry < REQ_RETRIES; retry++)
    {
        ch = curl_easy_init();
        if(ch)
        {
            curl_easy_setopt(ch, CURLOPT_URL, url);
            curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
            curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);
            curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, NULL);
            curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *)f);
            curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, err_buf);
            curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, REQ_CONNECT_TIMEOUT);
            curl_easy_setopt(ch, CURLOPT_TIMEOUT, REQ_FILE_TIMEOUT);
            curl_easy_setopt(ch, CURLOPT_HTTPGET, 1);
            if(slhdr) {
                curl_easy_setopt(ch, CURLOPT_HTTPHEADER, slhdr);
            }
            if(sldns) {
                curl_easy_setopt(ch, CURLOPT_RESOLVE, sldns);
            }
            if(unum_config.ca_file) {
                curl_easy_setopt(ch, CURLOPT_CAINFO, unum_config.ca_file);
            }
            if(is_provisioned()) {
                curl_easy_setopt(ch, CURLOPT_SSLCERTTYPE, "PEM");
                curl_easy_setopt(ch, CURLOPT_SSLCERT, PROVISION_CERT_PNAME);
                curl_easy_setopt(ch, CURLOPT_SSLKEY, PROVISION_KEY_PNAME);
            }

            err = curl_easy_perform(ch);

            if(err != 0) {
                log("%s: error: (%d) %s\n",
                    __func__, err, curl_easy_strerror(err));
                log("%s: error info: %s\n", __func__, err_buf);
            }

            curl_easy_cleanup(ch);
        }
        else
        {
            log("%s: failed to get curl handle\n", __func__);
        }
    }

    if(slhdr) {
        curl_slist_free_all(slhdr);
    }
    if(sldns) {
        curl_slist_free_all(sldns);
    }
    fclose(f);

    log("%s: done on try %d, err %d\n", __func__, retry, err);
    return err;
}

// CURL write function (stores response data in http_rsp ptr that
// is passed by curl as the userdata param)
static size_t write_func(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    size_t len;
    http_rsp *rsp;
    http_rsp **p_rsp;

    len = size * nmemb;
    p_rsp = (http_rsp **)userdata;
    rsp = *p_rsp;

    *p_rsp = rsp = alloc_rsp(rsp, rsp->len + len + 1);
    if(!rsp) {
        return 0; // will cause curl to fail with write error
    }
    memcpy(&(rsp->data[rsp->len]), ptr, len);
    rsp->len += len;
    rsp->data[rsp->len] = 0;

    return len;
}

// CURL write function to determine the size of the
// requested file, but does not store any of the data.
static size_t speedtest_chunk(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;

  size_t *total_size = (size_t *)userp;
  *total_size += realsize;

  return realsize;
}

// Perform HTTP POST or GET request.
// This is the worker function used by http_post/http_get wrappers.
// The headers are passed as double 0 terminated multi-string.
// Returns pointer to the http_rsp if sucessful, NULL if unable to perform
// the request.
// The caller must free the http_rsp when it is no longer needed.
#define HTTP_REQ_TYPE_GET  0
#define HTTP_REQ_TYPE_POST 1
#define HTTP_REQ_TYPE_PUT  2
#define HTTP_REQ_TYPE_MASK 0x0000ffff
#define HTTP_REQ_FLAGS_CAPTURE_HEADERS 0x00010000
#define HTTP_REQ_FLAGS_NO_RETRIES      0x00020000
#define HTTP_REQ_FLAGS_SHORT_TIMEOUT   0x00030000
static http_rsp *http_req(char *url, char *headers,
                          int type, char *data, int len)
{
    CURL *ch;
    int err;
    int retry;
    http_rsp *rsp;
    long resp_code;
    char *hdr;
    char *typestr;
    int num_retries = REQ_RETRIES;
    struct curl_slist *slhdr = NULL;
    struct curl_slist *sldns = NULL;
    char err_buf[CURL_ERROR_SIZE] = "";

    rsp = alloc_rsp(NULL, RSP_BUF_SIZE);
    if(!rsp) {
        log("%s: url <%s>, error allocating response buffer\n");
        return NULL;
    }

    ch = curl_easy_init();
    if(!ch) {
        free_rsp(rsp);
        log("%s: url <%s>, error curl_easy_init() has failed\n");
        return NULL;
    }

    if((type & HTTP_REQ_TYPE_MASK) == HTTP_REQ_TYPE_GET) {
        curl_easy_setopt(ch, CURLOPT_HTTPGET, 1);
        typestr = "GET";
    } else if((type & HTTP_REQ_TYPE_MASK) == HTTP_REQ_TYPE_POST) {
        curl_easy_setopt(ch, CURLOPT_POST, 1);
        typestr = "POST";
    } else if((type & HTTP_REQ_TYPE_MASK) == HTTP_REQ_TYPE_PUT) {
        curl_easy_setopt(ch, CURLOPT_CUSTOMREQUEST, "PUT");
        typestr = "PUT";
    } else {
        typestr = "UNKNOWN";
    }

    log("%s: %08x %s url <%s>\n", __func__, ch, typestr, url);

    curl_easy_setopt(ch, CURLOPT_URL, url);
    curl_easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
    curl_easy_setopt(ch, CURLOPT_FOLLOWLOCATION, 1);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, write_func);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, &rsp);
    curl_easy_setopt(ch, CURLOPT_ERRORBUFFER, err_buf);
    if ((type & HTTP_REQ_FLAGS_SHORT_TIMEOUT) == HTTP_REQ_FLAGS_SHORT_TIMEOUT) {
        curl_easy_setopt(ch, CURLOPT_TIMEOUT, REQ_API_TIMEOUT_SHORT);
        // Give connecting the whole timeout, but just reuse the constant
        curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, REQ_API_TIMEOUT_SHORT);
    } else {
        curl_easy_setopt(ch, CURLOPT_TIMEOUT, REQ_API_TIMEOUT);
        curl_easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, REQ_CONNECT_TIMEOUT);
    }
    if((type & HTTP_REQ_FLAGS_CAPTURE_HEADERS) != 0) {
        curl_easy_setopt(ch, CURLOPT_HEADERDATA, &rsp);
    }
    if(headers)
    {
        struct curl_slist *slold = NULL;
        for(hdr = headers; *hdr != 0; hdr += strlen(hdr) + 1)
        {
            log("%s: %08x hdr: '%s'\n", __func__, ch, hdr);
            slold = slhdr;
            if((slhdr = curl_slist_append(slhdr, hdr)) == NULL) {
                break;
            }
        }
        if(slhdr) {
            curl_easy_setopt(ch, CURLOPT_HTTPHEADER, slhdr);
        } else {
            log("%s: %08x failed to add headers, ignoring\n", __func__, ch);
            if(slold) {
                curl_slist_free_all(slold);
            }
        }
    }
    if(conncheck_no_dns())
    {
        int ii;
        struct curl_slist *slold = NULL;
        for(ii = 0; dns_map[ii] != NULL; ++ii)
        {
            slold = sldns;
            if((sldns = curl_slist_append(sldns, dns_map[ii])) == NULL) {
                break;
            }
        }
        if(ii > 0 && !sldns) {
            log("%s: failed to add static DNS entries, ignoring\n", __func__);
            if(slold) {
                curl_slist_free_all(slold);
            }
        } else {
            curl_easy_setopt(ch, CURLOPT_RESOLVE, sldns);
        }
    }
    if(unum_config.ca_file) {
        curl_easy_setopt(ch, CURLOPT_CAINFO, unum_config.ca_file);
    }
    if(is_provisioned()) {
        curl_easy_setopt(ch, CURLOPT_SSLCERTTYPE, "PEM");
        curl_easy_setopt(ch, CURLOPT_SSLCERT, PROVISION_CERT_PNAME);
        curl_easy_setopt(ch, CURLOPT_SSLKEY, PROVISION_KEY_PNAME);
    }
    if(((type & HTTP_REQ_TYPE_MASK) == HTTP_REQ_TYPE_POST) ||
       ((type & HTTP_REQ_TYPE_MASK) == HTTP_REQ_TYPE_PUT))
    {
        log("%s: %08x len: %d, data: '%.*s%s'\n", __func__, ch, len,
            (len > MAX_LOG_DATA_LEN ? MAX_LOG_DATA_LEN : len), data,
            (len > MAX_LOG_DATA_LEN ? "..." : ""));
        curl_easy_setopt(ch, CURLOPT_POSTFIELDS, data);
        curl_easy_setopt(ch, CURLOPT_POSTFIELDSIZE, len);
    }

    if((type & HTTP_REQ_FLAGS_NO_RETRIES) != 0) {
        num_retries = 1;
    }

    for(retry = 0; retry < num_retries; retry++)
    {
        err = curl_easy_perform(ch);
        if(err) {
            log("%s: %08x error (%d) %s\n",
                __func__, ch, err, curl_easy_strerror(err));
            log("%s: %08x error info: %s\n", __func__, ch, err_buf);
            // Refresh DNS servers, it might help with libc stale/cached DNS servers
            res_init();
            continue;
        }

        err = curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &resp_code);
        if(err) {
            log("%s: %08x getinfo error (%d) %s\n",
                __func__, ch, err, curl_easy_strerror(err));
            log("%s: %08x getinfo error info: %s\n", __func__, ch, err_buf);
            break;
        }

        log("%s: %08x OK, reply: '%.*s%s'\n", __func__, ch,
            (rsp->len > MAX_LOG_DATA_LEN ? MAX_LOG_DATA_LEN : rsp->len),
            rsp->data,
            (rsp->len > MAX_LOG_DATA_LEN ? "..." : ""));
        log("%s: %08x rsp code: %ld\n", __func__, ch, resp_code);
        rsp->code = resp_code;

        break;
    }

    curl_easy_cleanup(ch);
    if(slhdr != NULL) {
        curl_slist_free_all(slhdr);
    }
    if(sldns != NULL) {
        curl_slist_free_all(sldns);
    }

    if(err) {
        free_rsp(rsp);
        return NULL;
    }

    return rsp;
}

// Perform POST request
// The headers are passed as double 0 terminated multi-string.
// Returns pointer to the http_rsp if sucessful, NULL if unable to perform
// the request.
// The caller must free the http_rsp when it is no longer needed.
http_rsp *http_post(char *url, char *headers, char *data, int len)
{
    return http_req(url, headers, HTTP_REQ_TYPE_POST, data, len);
}
// The same as http_post(), but response headers are captured too
http_rsp *http_post_all(char *url, char *headers, char *data, int len)
{
    return http_req(url, headers,
                    HTTP_REQ_TYPE_POST | HTTP_REQ_FLAGS_CAPTURE_HEADERS,
                    data, len);
}
// The same as http_post(), but only tries once even if cannot connect
http_rsp *http_post_no_retry(char *url, char *headers, char *data, int len)
{
    return http_req(url, headers,
                    HTTP_REQ_TYPE_POST | HTTP_REQ_FLAGS_NO_RETRIES,
                    data, len);
}
// The same as http_post(), but with a short timeout and no retries.
http_rsp *http_post_short_timeout(char *url, char *headers, char *data, int len)
{
    return http_req(url, headers,
                    HTTP_REQ_TYPE_POST | HTTP_REQ_FLAGS_SHORT_TIMEOUT | HTTP_REQ_FLAGS_NO_RETRIES,
                    data, len);
}

// Perform PUT request
// The headers are passed as double 0 terminated multi-string.
// Returns pointer to the http_rsp if sucessful, NULL if unable to perform
// the request.
// The caller must free the http_rsp when it is no longer needed.
http_rsp *http_put(char *url, char *headers, char *data, int len)
{
    return http_req(url, headers, HTTP_REQ_TYPE_PUT, data, len);
}
// The same as above, but response headers are captured too
http_rsp *http_put_all(char *url, char *headers, char *data, int len)
{
    return http_req(url, headers,
                    HTTP_REQ_TYPE_PUT | HTTP_REQ_FLAGS_CAPTURE_HEADERS,
                    data, len);
}

// Perform GET request
// The headers are passed as double 0 terminated multi-string.
// Returns pointer to the http_rsp if sucessful, NULL if unable to perform
// the request.
// The caller must free the http_rsp when it is no longer needed.
http_rsp *http_get(char *url, char *headers)
{
    return http_req(url, headers, HTTP_REQ_TYPE_GET, NULL, 0);
}
// The same as above, but response headers are captured too
http_rsp *http_get_all(char *url, char *headers)
{
    return http_req(url, headers,
                    HTTP_REQ_TYPE_GET | HTTP_REQ_FLAGS_CAPTURE_HEADERS,
                    NULL, 0);
}
// The same as htt_get(), but only tries once even if cannot connect
http_rsp *http_get_no_retry(char *url, char *headers)
{
    return http_req(url, headers,
                    HTTP_REQ_TYPE_GET | HTTP_REQ_FLAGS_NO_RETRIES,
                    NULL, 0);
}

// Download file from a URL, without storing any data
// this is used to test speeds. returns 0 if successful
// error otherwise.
int http_download_test(char *url, long timeout_in_sec, size_t *returned_size)
{
    CURL *ch;
    CURLcode res;
    size_t total_size = 0;
    int ret = 0;
    long http_code = 0;

    ch = curl_easy_init();

    // Setup CURL options
    curl_easy_setopt(ch, CURLOPT_URL, url);
    curl_easy_setopt(ch, CURLOPT_WRITEFUNCTION, speedtest_chunk);
    curl_easy_setopt(ch, CURLOPT_WRITEDATA, (void *)&total_size);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "unum/v2 (libcurl; minim.co)");
    curl_easy_setopt(ch, CURLOPT_TIMEOUT, timeout_in_sec);

    // Start the download
    res = curl_easy_perform(ch);

    curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &http_code);

    // Handle errors
    if(res == CURLE_OPERATION_TIMEDOUT) {
        // Do not consider timed out requests as failed.
        log("%s: error: curl_easy_perform() operation timed out\n", __func__);
    } else if(res != CURLE_OK || (http_code / 100) != 2) {
        log("%s: error: curl_easy_perform() http_code: %ld | failed: %s\n",
            __func__, http_code, curl_easy_strerror(res));
        ret = -1;
    }
    *returned_size = total_size;

    curl_easy_cleanup(ch);
    return ret;
}

typedef struct transfer_status
{
    size_t total;
    size_t uploaded;
} transfer_status;

// CURL reader function to generate a set size of data
static size_t random_data_reader(void *ptr, size_t size,
                                 size_t nmemb, void *data)
{
    transfer_status *ts = data;
    if (ts->uploaded >= ts->total) {
        // We've reached our target, signal EOF.
        return 0;
    }
    // Calculate how much data we want to "read"
    size_t bytes_to_transfer = size * nmemb;

    // Tell curl the random data we want to upload
    memset(ptr, 125, bytes_to_transfer);

    // Save our progress
    ts->uploaded += bytes_to_transfer;

    return bytes_to_transfer;
}

// Uploads random data to an endpoint to test the upload speed
// returns 0 if successful, -1 otherwise.
int http_upload_test(char *url, long timeout_in_sec, size_t *uploaded_size)
{
    CURL *ch;
    CURLcode res;
    int ret = 0;
    long http_code = 0;

    // Setup our transfer status object
    struct transfer_status ts;
    ts.total = 4000L * 4000L; // arbitrary amount to upload
    ts.uploaded = 0;

    ch = curl_easy_init();

    // Setup CURL options
    curl_easy_setopt(ch, CURLOPT_URL, url);
    curl_easy_setopt(ch, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(ch, CURLOPT_READFUNCTION, random_data_reader);
    curl_easy_setopt(ch, CURLOPT_READDATA, (void *)&ts);
    curl_easy_setopt(ch, CURLOPT_USERAGENT, "unum/v2 (libcurl; minim.co)");
    curl_easy_setopt(ch, CURLOPT_TIMEOUT, timeout_in_sec);

    // Start the upload
    res = curl_easy_perform(ch);

    curl_easy_getinfo(ch, CURLINFO_RESPONSE_CODE, &http_code);

    // Handle errors
    if (res == CURLE_OPERATION_TIMEDOUT) {
        // Don't consider timed out requests as failed.
        log("%s: error: curl_easy_perform() operation timed out\n", __func__);
    } else if(res != CURLE_OK || (http_code / 100) != 2) {
        log("%s: error: curl_easy_perform() http_code: %ld | failed: %s\n",
            __func__, http_code, curl_easy_strerror(res));
        ret = -1;
    }

    *uploaded_size = ts.uploaded;

    curl_easy_cleanup(ch);
    return ret;
}

// Subsystem init function (nothing to do here yet)
int http_init(int level)
{
    int err;
    if(level != INIT_LEVEL_HTTP) {
        return 0;
    }
    log("%s: init curl\n", __func__);
    // Init curl
    err = curl_global_init(CURL_GLOBAL_ALL);
    if(err != 0) {
        log("%s: curl global init has failed, error %d\n", __func__, err);
    }

    return 0;
}
