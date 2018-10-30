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

// unum speedtest code

#include "unum.h"

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_STDOUT
//#define LOG_DBG_DST LOG_DST_STDOUT

#define SPEEDTEST_PROTO_VER "v2.1"

#define SPEEDTEST_REMOTE_URL_BASE      "https://api.minim.co"
#define SPEEDTEST_REMOTE_TELEMETRY_URL "%s/v3/unums/%s/speed_tests"
#define SPEEDTEST_REMOTE_SETTINGS_URL  "%s/v3/unums/%s/speed_tests/settings"

// Settings struct with hard-coded defaults.
// If Unum fails to obtain fresh settings from the Minim API during speedtest
// setup, it will fall back on these defaults so it may operate without
// connectivity to the Minim platform.
typedef struct speedtest_settings
{
    // Number of samples to attempt to collect for each test type.
    int latency_samples;
    int transfer_samples;
    // Number of data transfer threads to start.
    int transfer_concurrency;
    // Used as the basis for how large each transfer should be.
    // This parameter is called "ookla_size" in the speedtest settings payload.
    int transfer_size_base;
    // Base limit in seconds for how long the speedtest is allowed to run.
    // Note that this is not a hard limit and the whole speedtest normally
    // takes between 1.5x and 2x this value.
    int time_limit_sec;
    // Each transfer is allowed a little more than half of the total
    // time limit.
    int transfer_time_limit_sec;
    // Endpoint configuration.
    int endpoint_port;
    char endpoint_domain[64];
    char endpoint_protocol[32];
    // Resolved address for the configured endpoint above.
    struct sockaddr s_addr;
    struct sockaddr_in *s_addr_in_ptr;
    // Either TRUE if the agent is activated, otherwise FALSE.
    char online;
} speedtest_settings;

// Endpoint URLs for communicating with the remote Minim API.
static char endpoint_settings_url[256];
static char endpoint_telemetry_url[256];

// All concurrent reads and writes of any static state should be guarded by
// the "state_m" mutex.
static UTIL_MUTEX_t state_m = UTIL_MUTEX_INITIALIZER;
static UTIL_EVENT_t all_workers_started = UTIL_EVENT_INITIALIZER;
static UTIL_EVENT_t all_workers_finished = UTIL_EVENT_INITIALIZER;

// Current speedtest results
static int download_speed_kbps = -1;
static int upload_speed_kbps = -1;
static int latency_ms = -1;

// Speedtest settings
static speedtest_settings settings;
// Contains the state of the currently running speedtest.
// Values below are modified in concurrent operations.
static int running_workers;
static int worker_error;
static size_t total_bytes_transferred;
static unsigned long start_time_in_ms;
static unsigned long end_time_in_ms;


// Pointer to a function that performs a portion of an overall speedtest.
typedef int (*speedtest)(void);
// Forward declarations for our speedtest routines.
static int speedtest_latency();
static int speedtest_download();
static int speedtest_upload();
// All known tests.
static speedtest speedtests[] = {
    speedtest_latency,
    speedtest_download,
    speedtest_upload,
};

// Default speedtest settings, used when settings cannot be fetched from the
// remote Minim API.
static speedtest_settings speedtest_default_settings() {
    speedtest_settings cfg = {
        .latency_samples = 4,
        .transfer_samples = 4,
        .transfer_concurrency = 4,
        .transfer_size_base = 4000,
        .time_limit_sec = 20,
        .transfer_time_limit_sec = 11,
        .endpoint_port = 80,
        .endpoint_domain = "speedtest-server.starry.com",
        .endpoint_protocol = "http://",
        .online = FALSE
    };
    return cfg;
}

// Set URLs for the remote Minim API.
static void set_remote_urls()
{
    // Reads only, but no synchronization-- not thread safe.
    snprintf(endpoint_settings_url, sizeof(endpoint_settings_url),
        SPEEDTEST_REMOTE_SETTINGS_URL,
        (unum_config.url_prefix ?
            unum_config.url_prefix : SPEEDTEST_REMOTE_URL_BASE),
        util_device_mac());
    snprintf(endpoint_telemetry_url, sizeof(endpoint_telemetry_url),
        SPEEDTEST_REMOTE_TELEMETRY_URL,
        (unum_config.url_prefix ?
            unum_config.url_prefix : SPEEDTEST_REMOTE_URL_BASE),
        util_device_mac());
}

// Populates res with the full download URL.
static int snprint_download_url(char *res, size_t sz)
{
    // Reads only, but no synchronization-- not thread safe.
    return snprintf(res, sz,
        "%s" IP_PRINTF_FMT_TPL ":%i/speedtest/random%ix%i.jpg",
        settings.endpoint_protocol,
        IP_PRINTF_ARG_TPL(&settings.s_addr_in_ptr->sin_addr),
        settings.endpoint_port,
        settings.transfer_size_base, settings.transfer_size_base);
}

// Fills res with the configured upload url.
static int snprint_upload_url(char *res, size_t sz)
{
    // Reads only, but no synchronization-- not thread safe.
    return snprintf(res, sz,
        "%s" IP_PRINTF_FMT_TPL ":%i/speedtest/upload.php",
        settings.endpoint_protocol,
        IP_PRINTF_ARG_TPL(&settings.s_addr_in_ptr->sin_addr),
        settings.endpoint_port);
}

// Reset result variables.
// Not thread safe. Only call this from the main speedtest thread.
static void reset_results()
{
    download_speed_kbps = -1;
    upload_speed_kbps = -1;
    latency_ms = -1;
}

// Reset shared static state.
// Not thread safe. Only call this from the main speedtest thread.
static void reset_counters()
{
    total_bytes_transferred = 0;
    running_workers = 0;
    worker_error = 0;
}

// Populates cfg with data stored in data_root.
// Returns <0 if it fails on a required field.
static int speedtest_read_settings(json_t *data_root, speedtest_settings *cfg)
{
    if(cfg == NULL || data_root == NULL) {
        return -1;
    }
    const char *tmp;
    json_t *val;
    if((val = json_object_get(data_root, "endpoint_domain")) != NULL &&
        (tmp = json_string_value(val)) != NULL) {
        strncpy(cfg->endpoint_domain, tmp, sizeof(cfg->endpoint_domain));
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "endpoint_protocol")) != NULL &&
        (tmp = json_string_value(val)) != NULL) {
        strncpy(cfg->endpoint_protocol, tmp, sizeof(cfg->endpoint_protocol));
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "endpoint_port")) != NULL) {
        cfg->endpoint_port = (int)UTIL_MIN(json_integer_value(val), 65535);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "download_concurrency")) != NULL) {
        cfg->transfer_concurrency = (int)UTIL_MIN(json_integer_value(val), 8);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "latency_samples")) != NULL) {
        cfg->latency_samples = (int)UTIL_MIN(json_integer_value(val), 8);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "ookla_size")) != NULL) {
        cfg->transfer_size_base = (int)UTIL_MIN(
            json_integer_value(val), 8000);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "num_sizes")) != NULL) {
        cfg->transfer_samples = (int)UTIL_MIN(json_integer_value(val), 8);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "test_timeout_in_sec")) != NULL) {
        cfg->time_limit_sec = (int)UTIL_MIN(json_integer_value(val), 120);
    } else {
        return -1;
    }
    cfg->online = TRUE;
    return 0;
}

// Attempts to retrieve speedtest settings from the Minim API.
// If the device is not activated or does not have SSL certificates
// configured correctly, this call will fail, returning < 0.
// Not thread safe. Only call this before starting any worker threads.
static int speedtest_fetch_settings(speedtest_settings *cfg)
{
    if(cfg == NULL) {
        log("%s: fetch settings failed, invalid pointer received", __func__);
        return -1;
    }

    // Send a blocking GET request to the Minim API to get fresh settings.
    http_rsp *rsp = http_get(
        endpoint_settings_url,
        "Accept: application/json");
    if(rsp == NULL || (rsp->code / 100) != 2) {
        if(rsp) {
            free_rsp(rsp);
        }
        log("%s: request error, code %d%s\n",
            __func__, rsp ? rsp->code : 0, rsp ? "" : "(none)");
        return -1;
    }

    // Parse the received JSON payload.
    json_error_t error;
    json_t *data_root = json_loads(rsp->data, JSON_ALLOW_NUL, &error);
    free_rsp(rsp);
    if(!data_root) {
        log("%s: error parsing speedtest settings on line %d: %s\n",
            __func__, error.line, error.text);
        return -1;
    }

    int res = speedtest_read_settings(data_root, cfg);
    json_decref(data_root);
    if(res < 0) {
        log("%s: error parsing speedtest settings on line %d: %s\n",
            __func__, error.line, error.text);
        return -1;
    }

    // Allocate a bit more than half the timeout for each transfer test.
    cfg->transfer_time_limit_sec = (cfg->time_limit_sec / 2) + 1;

    return 0;
}

// Prints the current settings.
static void speedtest_print_settings()
{
    // Reads only, but no synchronization-- may not be completely thread safe.
    log("%s: agent(activated: %s)\n",
        __func__,
        settings.online == TRUE ?
            "true" : "false");
    log("%s: latency_samples(%i)\n", __func__, settings.latency_samples);
    log("%s: transfer_concurrency(%i)\n",
        __func__, settings.transfer_concurrency);
    log("%s: transfer_samples(%i)\n", __func__, settings.transfer_samples);
    log("%s: time_limit(%isec)\n", __func__, settings.time_limit_sec);
    log("%s: download_image_size(%ix%i)\n",
        __func__, settings.transfer_size_base, settings.transfer_size_base);
    log("%s: per_test_time_limit(%isec)\n",
        __func__, settings.transfer_time_limit_sec);
    if(settings.endpoint_domain == NULL ||
        settings.endpoint_protocol == NULL)
    {
        log("%s: cannot print endpoint settings, invalid pointer");
        return;
    }
    log("%s: endpoint(%s%s:%i)\n",
        __func__,
        settings.endpoint_protocol,
        settings.endpoint_domain, settings.endpoint_port);
    log("%s: endpoint_ipv4(%s" IP_PRINTF_FMT_TPL ":%i)\n",
        __func__, settings.endpoint_protocol,
        IP_PRINTF_ARG_TPL(&settings.s_addr_in_ptr->sin_addr),
        settings.endpoint_port);
}

// Resolve the IP address for the configured speedtest endpoint domain.
// Not thread safe. Should be called before starting any worker threads.
static int resolve_server_addr(speedtest_settings *cfg)
{
    cfg->s_addr_in_ptr = (struct sockaddr_in*)&cfg->s_addr;
    if(cfg->endpoint_domain == NULL) {
        return -1;
    }
    if(util_get_ip4_addr(cfg->endpoint_domain, &cfg->s_addr) < 0 ||
         cfg->s_addr.sa_family != AF_INET)
    {
        return -1;
    }
    return 0;
}

// Generic test procedure for initiating a multiple concurrent transfers and
// measuring the cumulative bytes transferred.
// This function spawns transfer_concurrency-1 number of threads using workfn
// as the thread entry point.
// If the test fails, the return value will be < 0.
static int speedtest_transfer(const char *descriptor, THRD_FUNC_t workfn,
    int *speed_kbps)
{
    reset_counters();
    UTIL_EVENT_RESET(&all_workers_started);
    UTIL_EVENT_RESET(&all_workers_finished);

    // Used as the parameter to the main thread's worker function.
    THRD_PARAM_t t_param = {{.int_val = TRUE}};

    int starting_workers = 0;
    while(starting_workers < settings.transfer_concurrency-1) {
        // Start individual worker threads.
        // NULL is used as the THRD_PARAM_t and can be subsequently checked
        // for inside the function workfn points to.
        // The function pointed at should wait to start its transfer until a
        // signal from the master worker indicates all workers are ready.
        // See the speedtest_transfer_worker function for an example.
        if(util_start_thrd("speedtest_transfer_worker", workfn, NULL, NULL) == 0) {
            ++starting_workers;
        }
    }

    // Give the other threads time to start and ready themselves.
    while(running_workers < starting_workers) {
        sleep(1);
    }

    // Call workfn in the current thread, this is our master worker.
    // Notice that this invocation of workfn receives a valid pointer
    // to THRD_PARAM_t instead of NULL.
    // The function pointed at should signal to all the worker threads
    // to start their individual transfers.
    // This call should block until all worker threads are done processing.
    (*workfn)(&t_param);

    log("%s: %s complete after %lums and %lubytes\n",
        __func__, descriptor,
        (end_time_in_ms - start_time_in_ms), total_bytes_transferred);

    if(worker_error < 0) {
        log("%s: %s finished with errors, discarding result\n",
            __func__, descriptor);
        return -1;
    }
    // Test was successful, calculate the speed in kbps.
    unsigned long duration_in_mseconds = end_time_in_ms - start_time_in_ms;
    int speed = (total_bytes_transferred / duration_in_mseconds) * 8;

    *speed_kbps = speed;

    log("%s: %s speed: %ikbps\n", __func__, descriptor, speed);

    return 0;
}

// A function pointer type for http_download_test and http_upload_test.
// Parameters, in order: the URL to fetch, the timeout in seconds, and finally,
// the size_t out parameter which is populated with the number of bytes
// transferred during the test.
typedef int (*transfer_func)(char*, long, size_t*);

// Performs one set of transfers generically by delegating most
// of the work to the transfer_func function pointer, which should be
// either http_download_test or http_upload_test.
static void speedtest_transfer_worker(const char *descriptor,
    transfer_func transfer_fn, char *url, THRD_PARAM_t *p)
{
    UTIL_MUTEX_TAKE(&state_m);
    int worker_id = ++running_workers;
    UTIL_MUTEX_GIVE(&state_m);

    // Kick off all workers if we are the main worker thread, otherwise
    // wait for the kick off.
    if(p && p->int_val) {
        // Set the start time from only the main worker thread.
        // Note that the end time is set by the last worker to finish.
        start_time_in_ms = util_time(1000);

        UTIL_EVENT_SETALL(&all_workers_started);

        // Print the target URL, but only once.
        log("%s: %s %s\n", __func__, descriptor, url);
    } else {
        UTIL_EVENT_WAIT(&all_workers_started);
    }

    unsigned long start_ms = util_time(1000);
    unsigned long checkpoint_ms = 0;
    int x;
    for(x = 0; x < settings.transfer_samples; ++x)
    {
        // Break if any worker has hit an error.
        if(worker_error < 0) {
            break;
        }
        log("%s(#%i): starting %s %i of %i \n",
            __func__, worker_id, descriptor, x+1, settings.transfer_samples);

        // Invoke the received transfer_func.
        size_t bytes_transferred;
        if((*transfer_fn)(url,
            settings.transfer_time_limit_sec, &bytes_transferred) < 0)
        {
            log("%s(#%i): %s of chunk failed\n",
                __func__, worker_id, descriptor);

            __sync_synchronize();
            worker_error = -1;
        }

        UTIL_MUTEX_TAKE(&state_m);
        total_bytes_transferred += bytes_transferred;
        UTIL_MUTEX_GIVE(&state_m);

        // Break if we are within 1 second of hitting our deadline.
        checkpoint_ms = util_time(1000);
        if(checkpoint_ms - start_ms >
            (settings.transfer_time_limit_sec - 1) * 1000)
        {
            break;
        }
    }

    // Wind down this worker.
    UTIL_MUTEX_TAKE(&state_m);
    --running_workers;
    if(running_workers == 0) {
        // The last worker (the current thread) finished; record the end time
        end_time_in_ms = checkpoint_ms;
        // and signal to all threads that the transfer test is complete.
        UTIL_EVENT_SETALL(&all_workers_finished);
    }
    UTIL_MUTEX_GIVE(&state_m);

    // Wait till all workers are done before shutting any of them down.
    // This is necessary to avoid negatively impacting any active transfers
    // still running on other threads by causing a potentially expensive
    // thread cleanup operation.
    UTIL_EVENT_TIMEDWAIT(&all_workers_finished, 60000);

    return;
}

// Serializes the current speedtest results as a JSON string.
// If a particular portion of the test is not complete, the recorded
// value will be NULL.
static int speedtest_results_to_json(char *out, int len)
{
    int download = download_speed_kbps;
    int upload = upload_speed_kbps;
    int latency = latency_ms;
    JSON_OBJ_TPL_t tpl_data = {
        { "download_speed_kbps",
            {.type = JSON_VAL_PINT, {.pi = download >= 0 ? &download : NULL}}},
        { "upload_speed_kbps",
            {.type = JSON_VAL_PINT, {.pi = upload   >= 0 ? &upload   : NULL}}},
        { "latency_ms",
            {.type = JSON_VAL_PINT, {.pi = latency  >= 0 ? &latency  : NULL}}},
        { NULL }
    };

    JSON_OBJ_TPL_t tpl_base = {
        { "speed_test", { .type = JSON_VAL_OBJ, { .o = tpl_data }}},
        { NULL }
    };

    char *json = util_tpl_to_json_str(tpl_base);
    if(json == NULL) {
        return -1;
    }
    strncpy(out, json, len);
    util_free_json_str(json);
    return 0;
}

// Attempts to post the speedtest results to the Minim API.
// This function returns < 0 on failure.
static int speedtest_do_upload_results(const char *jstr, char short_timeout)
{
    http_rsp *rsp =
        (short_timeout == TRUE ? http_post_short_timeout : http_post)(
            endpoint_telemetry_url,
            "Content-Type: application/json\0"
            "Accept: application/json\0",
            (char*)jstr, strlen(jstr));
    int ret = 0;
    if(rsp == NULL || (rsp->code / 100) != 2) {
        log("%s: request error, code %d%s\n",
            __func__, rsp ? rsp->code : 0, rsp ? "" : "(none)");
        ret = -1;
    }
    if(rsp) {
        free_rsp(rsp);
    }

    return ret;
}

// Upload the current test results, but timeout quickly and do not retry.
static int speedtest_upload_results_failfast()
{
    char jstr[200];
    speedtest_results_to_json(jstr, 200);
    return speedtest_do_upload_results(jstr, TRUE);
}

// Uploads results to the Minim API, retrying if it fails.
static int speedtest_upload_results()
{
    // Short-circuit immediately if the agent is offline.
    if(settings.online != TRUE) {
        log("%s: agent is not activated, not uploading results.\n",
            __func__);
        return -1;
    }
    log("%s: uploading results to the Minim API\n", __func__);

    char jstr[200];
    speedtest_results_to_json(jstr, 200);
    // Short timeout on the first try.
    if(speedtest_do_upload_results(jstr, TRUE) < 0) {
        // Upload failed.
        // Since the short timeout implies no retry, we retry the second
        // time ourselves.
        log("%s: error uploading results, retrying in 1 second...\n",
                __func__);
        sleep(1);
        // Use the longer timeout on the second try.
        speedtest_do_upload_results(jstr, FALSE);
    }

    return 0;
}

// Individual worker thread procedure for the download speed test.
static void speedtest_download_worker(THRD_PARAM_t *p)
{
    char url[255];
    snprint_download_url(url, sizeof(url));
    // Delegate most work to the generic speedtest_transfer_worker.
    speedtest_transfer_worker("download", http_download_test, url, p);
}

// Individual worker thread procedure for the upload speed test.
static void speedtest_upload_worker(THRD_PARAM_t *p)
{
    char url[255];
    snprint_upload_url(url, sizeof(url));
    // Delegate most work to the generic speedtest_transfer_worker.
    speedtest_transfer_worker("upload", http_upload_test, url, p);
}

// Measures latency between the speedtest server and the agent.
// The result will be < 0 if the test fails.
static int speedtest_latency()
{
    unsigned long sum = 0;
    int x, samples = 0;
    for(x = 0; x < settings.latency_samples; x++) {
        int ret = util_ping(&settings.s_addr, 3);
        if(ret < 0) {
            continue;
        }
        sum += ret;
        ++samples;
    }
    if(samples <= 0) {
        log("%s: no samples to calculate latency\n", __func__);
        return -1;
    }
    int latency = (int)(sum / samples);
    if(latency < 1) {
        // Consider <1ms invalid.
        log("%s: unable to determine latency\n", __func__);
        return -2;
    }

    latency_ms = latency;
    log("%s: average latency: %ims\n", __func__, latency);
    return 0;
}

// Download speed test procedure.
static int speedtest_download()
{
    return speedtest_transfer("download",
        speedtest_download_worker, &download_speed_kbps);
}

// Upload speed test procedure.
static int speedtest_upload()
{
    return speedtest_transfer("upload",
        speedtest_upload_worker, &upload_speed_kbps);
}

// Performs the whole speed test, blocking until complete.
void speedtest_perform()
{
    log("%s: unum speedtest " SPEEDTEST_PROTO_VER "\n", __func__);

    reset_results();
    set_remote_urls();

    // Fetch the settings from the Minim API, if possible. If this fails, the
    // defaults defined at the top of this file will be used.
    if(speedtest_fetch_settings(&settings) < 0) {
        settings = speedtest_default_settings();
        log("%s: failed to fetch settings from remote API, "
            "continuing with defaults\n", __func__);
    }
    // Resolve the endpoint domain or give up.
    if(resolve_server_addr(&settings) < 0) {
        // Device is probably completely offline-- can't do anything but abort.
        log("%s: unable to resolve remote host %s, aborting\n",
            __func__, settings.endpoint_domain);

        return;
    }

    // Print details of the configured environment.
    speedtest_print_settings();

    if(settings.online) {
        log("%s: streaming results to Minim API as tests finish\n", __func__);
    }

    // Iterate over individual tests, running each one at a time.
    int i;
    int limit = UTIL_ARRAY_SIZE(speedtests);
    for(i = 0; i < limit; ++i) {
        speedtests[i]();
        if(i != limit-1) {
            // Stream results except for the last iteration-- the results
            // will be uploaded after the test is over anyway.
            speedtest_upload_results_failfast();
        }
        // Let other threads have a turn.
        sched_yield();
    }

    // Deal with the various possible final states.
    if(settings.online == TRUE) {
        if(speedtest_upload_results(3) < 0) {
            log("%s: results not uploaded to server\n", __func__);
        } else {
            log("%s: results successfully uploaded to server\n", __func__);
        }
    } else {
        log("%s: agent is not activated, not uploading results\n", __func__);
    }

    char results[150];
    speedtest_results_to_json(results, sizeof(results));
    log("%s: results: %s\n", __func__, results);
}

// Starts a speedtest on a new thread.
void cmd_speedtest()
{
    // Perform the test in a new thread so this command can return immediately.
    util_start_thrd("speedtest_perform", speedtest_perform, NULL, NULL);
}
