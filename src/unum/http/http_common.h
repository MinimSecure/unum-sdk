// (c) 2017-2019 minim.co
// unum helper utils include file

#ifndef _HTTP_COMMON_H
#define _HTTP_COMMON_H

// Initial allocation unit size for the http_rsp
// If data does not fit, the buffer is realloced to the double
// of its previous size.
#define RSP_BUF_SIZE 1024

// Structure containing HTTP request reply for GET/POST requests
typedef struct {
    int code;           // response code
    int len;            // length of the response data
    int size;           // size of the data buffer
    unsigned long time; // connection time
    char data[0]; // response data
    // The http_rsp is allocated with extra space below
    // for storing the response data
} http_rsp;

// Allocate (or reallocate) http_rsp structure that can fit
// requested amount of data. If NULL is returned then the system
// failed to allocate the buffer and the old buffer is no longer valid.
static __inline__ http_rsp *alloc_rsp(http_rsp *old_rsp, int new_len)
{
    int old_size = 0;
    int new_size = RSP_BUF_SIZE;
    http_rsp *new_rsp = NULL;

    if(old_rsp) {
        new_size = old_size = old_rsp->size;
    }
    while(new_len > new_size) {
        new_size *= 2;
    }
    if(old_rsp && old_size < new_size) {
        new_rsp = (http_rsp *)UTIL_REALLOC(old_rsp, new_size + sizeof(http_rsp));
        if(!new_rsp) {
            UTIL_FREE(old_rsp);
            return NULL;
        }
        new_rsp->size = new_size;
    } else if(!old_rsp) {
        new_rsp = (http_rsp *)UTIL_MALLOC(new_size + sizeof(http_rsp));
        new_rsp->code = 0;
        new_rsp->len = 0;
        new_rsp->size = new_size;
    } else {
        new_rsp = old_rsp;
    }

    return new_rsp;
}

// Free http_rsp structure
static __inline__ void free_rsp(http_rsp *old_rsp)
{
    UTIL_FREE(old_rsp);
}

// Returns NULL-teminated array of strings w/ hardcoded DNS names mapping
// http subsystem will use if DNS is not working. The entries are in
// the HOST:PORT:ADDRESS[,ADDRESS]... format.
char **http_dns_name_list(void);

// Download file from a URL
// The headers are passed as double 0 terminated multi-string.
// Returns 0 if successful error otherwise
int http_get_file(char *url, char *headers, char *file);

// Perform POST request
// The headers are passed as double 0 terminated multi-string.
// Returns pointer to the http_rsp if sucessful, NULL if unable to perform
// the request.
// The caller must free the http_rsp when it is no longer needed.
// The ..._all version of the function captures the response headers too.
http_rsp *http_post(char *url, char *headers, char *data, int len);
http_rsp *http_post_short_timeout(char *url, char *headers, char *data, int len);
http_rsp *http_post_all(char *url, char *headers, char *data, int len);
http_rsp *http_post_no_retry(char *url, char *headers, char *data, int len);

// Perform PUT request
// The headers are passed as double 0 terminated multi-string.
// Returns pointer to the http_rsp if sucessful, NULL if unable to perform
// the request.
// The caller must free the http_rsp when it is no longer needed.
// The ..._all version of the function captures the response headers too.
http_rsp *http_put(char *url, char *headers, char *data, int len);
http_rsp *http_put_all(char *url, char *headers, char *data, int len);

// Perform GET request
// The headers are passed as double 0 terminated multi-string.
// Returns pointer to the http_rsp if sucessful, NULL if unable to perform
// the request.
// The caller must free the http_rsp when it is no longer needed.
// The ..._all version of the function captures the response headers too.
http_rsp *http_get(char *url, char *headers);
http_rsp *http_get_all(char *url, char *headers);
http_rsp *http_get_no_retry(char *url, char *headers);
http_rsp *http_get_conn_time(char *url, char *headers);

// Download file from a URL, without storing any data
// this is used to test speeds. returns 0 if successful
// error otherwise.
// Pass 0 as timeout_in_sec to disable the timeout completely.
int http_download_test(char *ifname, char *url,
                       long timeout_in_sec, size_t *size);

// Uploads random data to an endpoint to test the upload speed
// returns 0 if successful, -1 otherwise.
// Pass 0 as timeout_in_sec to disable the timeout completely.
int http_upload_test(char *ifname, char *url,
                     long timeout_in_sec, size_t *size);

// Subsystem init function
int http_init(int level);

#endif // _HTTP_COMMON_H
