// (c) 2017-2020 minim.co
// unum speedtest code

#include "unum.h"

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_STDOUT
//#define LOG_DBG_DST LOG_DST_STDOUT
/* In case normal logging doesn't work during the test */
//#undef log
//#define log printf

#define SPEEDTEST_PROTO_VER "v2.1"

#define NORMAL_SPEEDTEST_REMOTE_TELEMETRY_PATH "/v3/unums/%s/speed_tests"
#define NORMAL_SPEEDTEST_REMOTE_SETTINGS_PATH  "/v3/unums/%s/speed_tests/settings"
#define CAF_LATENCY_TEST_REMOTE_TELEMETRY_PATH "/v3/caf/unums/%s/caf_latency"
#define CAF_LATENCY_TEST_REMOTE_SETTINGS_PATH  "/v3/caf/unums/%s/caf_latency/settings"
#define CAF_DOWNLOAD_TEST_REMOTE_TELEMETRY_PATH "/v3/caf/unums/%s/caf_download"
#define CAF_DOWNLOAD_TEST_REMOTE_SETTINGS_PATH  "/v3/caf/unums/%s/caf_download/settings"
#define CAF_UPLOAD_TEST_REMOTE_TELEMETRY_PATH "/v3/caf/unums/%s/caf_upload"
#define CAF_UPLOAD_TEST_REMOTE_SETTINGS_PATH  "/v3/caf/unums/%s/caf_upload/settings"

#define TEST_JSON \
"{\
	\"endpoint_domain\": \"speedtest-server.starry.com\",\
	\"endpoint_protocol\": \"http://\",\
	\"endpoint_port\": 80,\
	\"download_concurrency\": 4,\
	\"latency_samples\": 5,\
	\"ookla_size\": 4000,\
	\"num_sizes\": 4,\
	\"test_timeout_in_sec\": 20,\
	\"ping_endpoints\": [{\
		\"domain\": \"8.8.8.8\",\
		\"samples\": 3\
	}, {\
		\"domain\": \"1.1.1.1\",\
		\"samples\": 3\
	}],\
	\"endpoints\": [{\
		\"domain\": \"speedtest.peregrinenetworks.net\",\
		\"protocol\": \"http://\",\
		\"port\": 8080\
	}, {\
		\"domain\": \"speed0.xcelx.net\",\
		\"protocol\": \"http://\",\
		\"port\": 8080\
	}, {\
		\"domain\": \"speedtest-server.starry.com\",\
		\"protocol\": \"http://\",\
		\"port\": 8080\
	}, {\
		\"domain\": \"mawbp1.speed.vzwnet.com\",\
		\"protocol\": \"http://\",\
		\"port\": 5060\
	}, {\
		\"domain\": \"speedtest.bos01.infrastructure.iboss.cloud\",\
		\"protocol\": \"http://\",\
		\"port\": 8080\
	}]\
}"


#define MAX_ENDPOINTS 5
#define MAX_DOMAIN_STRING_LEN   64
#define MAX_PROTOCOL_STIRNG_LEN 32

#define MAX_LATENCY 9999

// Ping Endpoint Struct
// Contains the domain (or IP), # of samples
// and then storage of the latency result 
typedef struct pingtest_endpoint
{
    char domain[MAX_DOMAIN_STRING_LEN];
    int samples;
    int latency_ms;
} pingtest_endpoint;

// Endpoint Struct
// Contains domain, port, protocol for a speedtest server
// Server sends MAX_SPEEDTEST_ENDPOINTS of these
typedef struct speedtest_endpoint
{
    int port;
    int invalid;
    char domain[MAX_DOMAIN_STRING_LEN];
    char protocol[MAX_PROTOCOL_STIRNG_LEN];
} speedtest_endpoint;

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
    // Speedtest servers
    struct speedtest_endpoint endpoints[MAX_ENDPOINTS];
    // Ping servers
    struct pingtest_endpoint ping_endpoints[MAX_ENDPOINTS];
    // Either TRUE if the agent is activated, otherwise FALSE.
    char online;
    // Optional test_id (used for caf testing)
    int test_id;
} speedtest_settings;

// Actual Test Server Parameters
speedtest_endpoint *endpoint;
struct sockaddr s_addr;
struct sockaddr_in *s_addr_in_ptr;

// Interface to run the test from (if NULL no binding)
static char *ifname = NULL;
static char ifname_buf[IFNAMSIZ];

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
static int complete = 0;

// Optional test id
static int test_id = 0;

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
static speedtest normal_speedtests[] = {
    speedtest_latency,
    speedtest_download,
    speedtest_upload,
};

// latency test only
static speedtest latency_speedtests[] = {
    speedtest_latency,
};

// download test only
static speedtest download_speedtests[] = {
    speedtest_download,
};

// upload test only
static speedtest upload_speedtests[] = {
    speedtest_upload,
};

// A pointer to the speedtest array for the current test
static speedtest *speedtests;

#define MIN_LATENCY_SAMPLES 1
#define MAX_LATENCY_SAMPLES 10
#define DEFAULT_LATENCY_SAMPLES 4

#define MIN_TRANSFER_SAMPLES 1
#define MAX_TRANSFER_SAMPLES 10
#define DEFAULT_TRANSFER_SAMPLES 4

#define MIN_TRANSFER_CONCURRENCY 1
#define MAX_TRANSFER_CONCURRENCY 10
#define DEFAULT_TRANSFER_CONCURRENCY 4

#define MIN_TRANSFER_SIZE_BASE 1000
#define MAX_TRANSFER_SIZE_BASE 10000
#define DEFAULT_TRANSFER_SIZE_BASE 4000

#define MIN_TIME_LIMIT 5
#define MAX_TIME_LIMIT 120
#define DEFAULT_TIME_LIMIT 20

#define MIN_XFER_TIME_LIMIT 5
#define MAX_XFER_TIME_LIMIT 120
#define DEFAULT_XFER_TIME_LIMIT 11

#define MIN_PORT 1
#define MAX_PORT 65535
#define DEFAULT_PORT 80

// Default speedtest settings, used when settings cannot be fetched from the
// remote Minim API.
static speedtest_settings speedtest_default_settings() {
    speedtest_settings cfg = {
        .latency_samples = DEFAULT_LATENCY_SAMPLES,
        .transfer_samples = DEFAULT_TRANSFER_SAMPLES,
        .transfer_concurrency = DEFAULT_TRANSFER_CONCURRENCY,
        .transfer_size_base = DEFAULT_TRANSFER_SIZE_BASE,
        .time_limit_sec = DEFAULT_TIME_LIMIT,
        .transfer_time_limit_sec = DEFAULT_XFER_TIME_LIMIT,
        .endpoints = {
            {
                .domain = "speedtest-server.starry.com",
                .protocol = "http://",
                .port = DEFAULT_PORT,
            }
        },
        .online = FALSE,
        .test_id = 0
    };
    return cfg;
}

// Set URLs for the remote Minim API and
// select the correct test array for the type of
// test requested
static int config_for_cmd(char * test_cmd)
{

    if(0 == strcmp(test_cmd, "speedtest")) {
        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_settings_url, sizeof(endpoint_settings_url),
                       NORMAL_SPEEDTEST_REMOTE_SETTINGS_PATH, util_device_mac());

        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_telemetry_url, sizeof(endpoint_telemetry_url),
                       NORMAL_SPEEDTEST_REMOTE_TELEMETRY_PATH, util_device_mac());

        speedtests = normal_speedtests;
        return UTIL_ARRAY_SIZE(normal_speedtests);
    } else if(0 == strcmp(test_cmd, "caf_latency")) {
        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_settings_url, sizeof(endpoint_settings_url),
                       CAF_LATENCY_TEST_REMOTE_SETTINGS_PATH, util_device_mac());

        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_telemetry_url, sizeof(endpoint_telemetry_url),
                       CAF_LATENCY_TEST_REMOTE_TELEMETRY_PATH, util_device_mac());

        speedtests = latency_speedtests;
        return UTIL_ARRAY_SIZE(latency_speedtests);
    } else if(0 == strcmp(test_cmd, "caf_download")) {
        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_settings_url, sizeof(endpoint_settings_url),
                       CAF_DOWNLOAD_TEST_REMOTE_SETTINGS_PATH, util_device_mac());

        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_telemetry_url, sizeof(endpoint_telemetry_url),
                       CAF_DOWNLOAD_TEST_REMOTE_TELEMETRY_PATH, util_device_mac());

        speedtests = download_speedtests;
        return UTIL_ARRAY_SIZE(download_speedtests);
    } else if(0 == strcmp(test_cmd, "caf_upload")) {
        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_settings_url, sizeof(endpoint_settings_url),
                       CAF_UPLOAD_TEST_REMOTE_SETTINGS_PATH, util_device_mac());

        util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_API,
                       endpoint_telemetry_url, sizeof(endpoint_telemetry_url),
                       CAF_UPLOAD_TEST_REMOTE_TELEMETRY_PATH, util_device_mac());

        speedtests = upload_speedtests;
        return UTIL_ARRAY_SIZE(upload_speedtests);
    }

    // we can't handle this command
    return -1;
}

// Populates res with the full download URL.
static int snprint_download_url(char *res, size_t sz)
{
    // Reads only, but no synchronization-- not thread safe.
    return snprintf(res, sz,
        "%s" IP_PRINTF_FMT_TPL ":%i/speedtest/random%ix%i.jpg",
        endpoint->protocol,
        IP_PRINTF_ARG_TPL(&s_addr_in_ptr->sin_addr),
        endpoint->port,
        settings.transfer_size_base, settings.transfer_size_base);
}

// Fills res with the configured upload url.
static int snprint_upload_url(char *res, size_t sz)
{
    // Reads only, but no synchronization-- not thread safe.
    return snprintf(res, sz,
        "%s" IP_PRINTF_FMT_TPL ":%i/speedtest/upload.php",
        endpoint->protocol,
        IP_PRINTF_ARG_TPL(&s_addr_in_ptr->sin_addr),
        endpoint->port);
}

// Reset result variables.
// Not thread safe. Only call this from the main speedtest thread.
static void reset_results()
{
    download_speed_kbps = -1;
    upload_speed_kbps = -1;
    latency_ms = -1;
    complete = 0;
    endpoint = NULL;
    test_id = 0;
}

// Reset shared static state.
// Not thread safe. Only call this from the main speedtest thread.
static void reset_counters()
{
    total_bytes_transferred = 0;
    running_workers = 0;
    worker_error = 0;
}

// Populates ping endpoint from JSON data
static void read_ping_endpoint_entry(speedtest_settings *cfg, const json_t *root,
                                     const char *domain, const char *samples,
                                     int *index)
{
    json_t *val;
    const char *tmp;

    if(cfg == NULL || root == NULL || domain == NULL || samples == NULL ||
       index == NULL)
    {
        log("%s: Invalid argument.\n", __func__);
        return;
    }

    if((val = json_object_get(root, domain)) != NULL &&
        (tmp = json_string_value(val)) != NULL)
    {
        strncpy(cfg->ping_endpoints[(*index)].domain, tmp,
                sizeof(cfg->ping_endpoints[(*index)].domain));
        cfg->ping_endpoints[(*index)].domain[
            sizeof(cfg->ping_endpoints[(*index)].domain) - 1] = '\0';

        if((val = json_object_get(root, samples)) != NULL) {
            cfg->ping_endpoints[(*index)].samples = json_integer_value(val) & 0xFFFF;
            UTIL_RANGE_CHECK(cfg->ping_endpoints[(*index)].samples, 
                             MIN_LATENCY_SAMPLES, MAX_LATENCY_SAMPLES);
            (*index)++;
        }

    }

    return;
}

// Populates cfg endpoint from JSON data
static void read_endpoint_entry(speedtest_settings *cfg, const json_t *root,
                                const char *domain, const char *protocol,
                                const char *port, int *index)
{
    json_t *val;
    const char *tmp;

    if(cfg == NULL || root == NULL || domain == NULL || protocol == NULL ||
       port == NULL || index == NULL)
    {
        log("%s: Invalid argument.\n", __func__);
        return;
    }

    if((val = json_object_get(root, domain)) != NULL &&
        (tmp = json_string_value(val)) != NULL)
    {
        strncpy(cfg->endpoints[(*index)].domain, tmp,
                sizeof(cfg->endpoints[(*index)].domain));
        cfg->endpoints[(*index)].domain[
            sizeof(cfg->endpoints[(*index)].domain) - 1] = '\0';

        cfg->endpoints[(*index)].invalid = 0;

        if((val = json_object_get(root, protocol)) != NULL &&
            (tmp = json_string_value(val)) != NULL) {
            strncpy(cfg->endpoints[(*index)].protocol, tmp,
                    sizeof(cfg->endpoints[(*index)].protocol));
            cfg->endpoints[(*index)].protocol[
                sizeof(cfg->endpoints[(*index)].protocol) - 1] = '\0';

            if((val = json_object_get(root, port)) != NULL) {
                cfg->endpoints[(*index)].port = json_integer_value(val) & 0xFFFF;
                UTIL_RANGE_CHECK(cfg->endpoints[(*index)].port, MIN_PORT, MAX_PORT);
                (*index)++;
            }
        }
    }

    return;
}

// Populates cfg with data stored in data_root.
// Returns <0 if it fails on a required field.
static int speedtest_read_settings(json_t *data_root, speedtest_settings *cfg)
{
    if(cfg == NULL || data_root == NULL) {
        return -1;
    }

    json_t *val;
    int endpoint_index = 0;
    int ping_endpoint_index = 0;

    read_endpoint_entry(cfg, data_root, "endpoint_domain", "endpoint_protocol",
                        "endpoint_port", &endpoint_index);
    if((val = json_object_get(data_root, "download_concurrency")) != NULL) {
        cfg->transfer_concurrency = json_integer_value(val) & 0xFFFF;
        UTIL_RANGE_CHECK(cfg->transfer_concurrency, MIN_TRANSFER_CONCURRENCY,
                         MAX_TRANSFER_CONCURRENCY);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "latency_samples")) != NULL) {
        cfg->latency_samples = json_integer_value(val) & 0xFFFF;
        UTIL_RANGE_CHECK(cfg->latency_samples, MIN_LATENCY_SAMPLES,
                         MAX_LATENCY_SAMPLES);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "ookla_size")) != NULL) {
        cfg->transfer_size_base = json_integer_value(val) & 0xFFFF;
        UTIL_RANGE_CHECK(cfg->transfer_size_base, MIN_TRANSFER_SIZE_BASE,
                         MAX_TRANSFER_SIZE_BASE);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "num_sizes")) != NULL) {
        cfg->transfer_samples = json_integer_value(val) & 0xFFFF;
        UTIL_RANGE_CHECK(cfg->transfer_samples, MIN_TRANSFER_SAMPLES,
                         MAX_TRANSFER_SAMPLES);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "test_timeout_in_sec")) != NULL) {
        cfg->time_limit_sec = json_integer_value(val) & 0xFFFF;
        UTIL_RANGE_CHECK(cfg->time_limit_sec, MIN_TIME_LIMIT, MAX_TIME_LIMIT);
    } else {
        return -1;
    }
    if((val = json_object_get(data_root, "endpoints")) != NULL) {
        json_t *entry;
        size_t entry_index;
        json_array_foreach(val, entry_index, entry)
        {
            if(endpoint_index >= MAX_ENDPOINTS) {
                break;
            }
            read_endpoint_entry(cfg, entry, "domain", "protocol", "port",
                                &endpoint_index);
        }
    } else {
        // This field is optional
    }
    if((val = json_object_get(data_root, "ping_endpoints")) != NULL) {
        json_t *entry;
        size_t entry_index;
        json_array_foreach(val, entry_index, entry)
        {
            if(ping_endpoint_index >= MAX_ENDPOINTS) {
                break;
            }
            read_ping_endpoint_entry(cfg, entry, "domain", "samples",
                                     &ping_endpoint_index);
        }
    } else {
        // This field is optional
    }

    if((val = json_object_get(data_root, "test_id")) != NULL) {
        cfg->test_id = json_integer_value(val);
    } else {
        // This field is optional
    }

    if(endpoint_index == 0) {
        log("%s: No endpoints found in JSON\n", __func__);
        return -1;
    } else if(endpoint_index < MAX_ENDPOINTS) {
        cfg->endpoints[endpoint_index].domain[0] = '\0';
    }
    if(ping_endpoint_index < MAX_ENDPOINTS) {
        cfg->ping_endpoints[ping_endpoint_index].domain[0] = '\0';
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
        "Accept: application/json\0");
    if(rsp == NULL || (rsp->code / 100) != 2) {
        log("%s: request error, code %d%s\n",
            __func__, rsp ? rsp->code : 0, rsp ? "" : "(none)");
        if(rsp) {
            free_rsp(rsp);
        }
        return -1;
    }

    // Parse the received JSON payload.
    json_error_t error;
    json_t *data_root = json_loads(rsp->data, JSON_ALLOW_NUL, &error);
    free_rsp(rsp);
    if(!data_root) {
        log("%s: error parsing json on line %d: %s\n",
            __func__, error.line, error.text);
        return -1;
    }

    int res = speedtest_read_settings(data_root, cfg);
    json_decref(data_root);
    if(res < 0) {
        log("%s: error parsing speedtest settings %d\n",
            __func__, res);
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
    int ii = 0;
    for(ii = 0; ii < MAX_ENDPOINTS &&
        settings.endpoints[ii].domain[0] != '\0'; ii++)
    {
        log("%s: endpoint(%s%s:%i)\n",
            __func__,
            settings.endpoints[ii].protocol,
            settings.endpoints[ii].domain,
            settings.endpoints[ii].port);
    }
    for(ii = 0; ii < MAX_ENDPOINTS &&
        settings.ping_endpoints[ii].domain[0] != '\0'; ii++)
    {
        log("%s: ping_endpoint(%s,%i)\n",
            __func__,
            settings.ping_endpoints[ii].domain,
            settings.ping_endpoints[ii].samples);
    }

    if(settings.test_id != 0) {
        log("%s: test_id(%i)\n", __func__, settings.test_id);
    }
}

// Resolve the IP address for the configured speedtest endpoint domain.
// Not thread safe. Should be called before starting any worker threads.
static int resolve_server_addr()
{
    s_addr_in_ptr = (struct sockaddr_in*)&s_addr;

    // If endpoint is NULL, use the first endpoint we received in the settings
    if(endpoint == NULL) {
        endpoint = &settings.endpoints[0];
    }

    if(endpoint->domain == NULL) {
        return -1;
    }

    if(util_get_ip4_addr(endpoint->domain, &s_addr) < 0 ||
         s_addr.sa_family != AF_INET)
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

    // Resolve Server Address for target
    if(resolve_server_addr() < 0) {
       // Device is probably completely offline-- can't do anything but abort.
       log("%s: unable to resolve only remote host %s, aborting\n",
           __func__, endpoint->domain);
       return -1;
    }

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
        } else {
            log("%s: speedtest worker failed to start\n", __func__);
            return -1;
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
        (unsigned long)(end_time_in_ms - start_time_in_ms),
        (unsigned long)total_bytes_transferred);

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
typedef int (*transfer_func)(char*, char*, long, size_t*);

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
        if((*transfer_fn)(ifname, url,
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
    int ii;
    int download = download_speed_kbps;
    int upload = upload_speed_kbps;
    int latency = latency_ms;

    // Template array for latency
    static JSON_VAL_TPL_t ping_latency_result_tpl[MAX_ENDPOINTS + 1] = {
        [0 ... (MAX_ENDPOINTS - 1)] = { .type = JSON_VAL_PINT},
        [ MAX_ENDPOINTS ] = { .type = JSON_VAL_END }
    };

    // Template array for ping endpoints
    static JSON_VAL_TPL_t ping_endpoint_result_tpl[MAX_ENDPOINTS + 1] = {
        [0 ... (MAX_ENDPOINTS - 1)] = { .type = JSON_VAL_STR},
        [ MAX_ENDPOINTS ] = { .type = JSON_VAL_END }
    };

    // Fill in Ping Endpoints + Latencies
    for(ii = 0; ii < MAX_ENDPOINTS &&
        settings.ping_endpoints[ii].domain[0] != '\0'; ii++)
    {
        ping_latency_result_tpl[ii].pi = &settings.ping_endpoints[ii].latency_ms;
        ping_endpoint_result_tpl[ii].s = settings.ping_endpoints[ii].domain;
    }

    // Mark Last Entry 
    ping_latency_result_tpl[ii].type = JSON_VAL_END;
    ping_endpoint_result_tpl[ii].type = JSON_VAL_END;


    JSON_OBJ_TPL_t tpl_data = {
        { "test_id",
            {.type = JSON_VAL_PINT, {.pi = test_id != 0 ? &test_id : NULL}}},
        { "download_speed_kbps",
            {.type = JSON_VAL_PINT, {.pi = download >= 0 ? &download : NULL}}},
        { "upload_speed_kbps",
            {.type = JSON_VAL_PINT, {.pi = upload   >= 0 ? &upload : NULL}}},
        { "latency_ms",
            {.type = JSON_VAL_PINT, {.pi = latency  >= 0 ? &latency : NULL}}},
        { "ping_endpoints",
            {.type = JSON_VAL_ARRAY, {.a = ping_endpoint_result_tpl}}},
        { "ping_latencies",
            {.type = JSON_VAL_ARRAY, {.a = ping_latency_result_tpl}}},
        { "interface",
            {.type = JSON_VAL_STR,  {.s = ifname}}},
        { "endpoint",
            {.type = JSON_VAL_STR,  {.s = endpoint != NULL ? endpoint->domain : NULL}}},
        { "complete",
            {.type = JSON_VAL_INT,  {.i = complete}}},
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
    if(len > 0) {
        strncpy(out, json, len);
        out[len - 1] = 0;
    }
    util_free_json_str(json);
    return 0;
}

// Attempts to post the speedtest results to the Minim API.
// This function returns < 0 on failure.
static int speedtest_do_upload_results(const char *jstr, char short_timeout)
{
    http_rsp *rsp =
        (short_timeout ? http_post_short_timeout : http_post)(
            endpoint_telemetry_url,
            "Content-Type: application/json\0"
            "Accept: application/json\0",
            (char*)jstr, strlen(jstr));
    int ret = 0;
    if(rsp == NULL || (rsp->code / 100) != 2) {
        log("%s: request error, code %d%s\n",
            __func__, (rsp ? rsp->code : 0), (rsp ? "" : "(none)"));
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
    char jstr[256];
    speedtest_results_to_json(jstr, sizeof(jstr));
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

    char jstr[256];
    speedtest_results_to_json(jstr, sizeof(jstr));
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

// Connects to sa on specified port and returns the time
// it took to connect in milliseconds
static int get_conn_time(struct sockaddr *sa, short port)
{
    struct sockaddr_in server;
    int retval = MAX_LATENCY, sock;
    unsigned long long before;
    char dst_addr[INET_ADDRSTRLEN];
    fd_set fdset;
    struct timeval tv;

    inet_ntop(AF_INET,
              &((struct sockaddr_in *)sa)->sin_addr,
              dst_addr, INET_ADDRSTRLEN);
    log_dbg("%s: Destination address: %s\n", __func__, dst_addr);

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        log("%s: Error: Could not create socket!\n", __func__);
        return retval;
    }

    server.sin_addr.s_addr = inet_addr(dst_addr);
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if((fcntl(sock, F_SETFL, O_NONBLOCK)) < 0) {
        log("%s: Error: fctnl G_SETFL failed! %s\n", __func__, strerror(errno));
        close(sock);
        return retval;
    }

    before = util_time(1000);
    connect(sock, (struct sockaddr *)&server, sizeof(server));

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    tv.tv_sec = 3;
    tv.tv_usec = 0;

    if(select(sock + 1, NULL, &fdset, NULL, &tv) == 1)
    {
        int so_error;
        socklen_t len = sizeof(so_error);

        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);

        if(so_error == 0) {
            retval = util_time(1000) - before;
        } else {
            log("%s: Error: Could not connect to server! %d\n", __func__, strerror(so_error));
        }

    } else {
        log("%s: Error: Could not connect to server! %s\n", __func__, strerror(errno));
    }

    close(sock);

    return retval;
}

// Performs count number of TCP connects to sa on specified port
// and returns the average time or -1 on failure.
static int speedtest_ping(struct sockaddr *sa, short port, int count)
{
    int ii;
    int time, retval = MAX_LATENCY;

    for(ii = 0; ii < count; ii++) {
        time = get_conn_time(sa, port);
        retval = UTIL_MIN(retval, time);

    }

    if(retval == MAX_LATENCY) {
        log("%s: Connect Failure\n", __func__);
        return -1;
    }

    return retval;
}

// Measures latency between the candidate speedtest servers and the agent.
// The actual speedtest server will be set to the candidate server with the
// lowest latency after this function is executed. If the result is < 0
// there were no candidate servers, their names could not be resolved
// or they were unreachable.
static int speedtest_latency()
{
    int i;

    int min_time = INT_MAX;
    int min_time_index = -1;

    for(i = 0; i < MAX_ENDPOINTS &&
        settings.endpoints[i].domain[0] != '\0'; i++)
    {
        int time;
        struct sockaddr sa;

        // Skip if this server is invalid
        if(settings.endpoints[i].invalid) {
            log("%s: server <%s> is invalid, skipping\n", __func__,
                settings.endpoints[i].domain);
            continue;
        }

        // Try to resolve name
        if(util_get_ip4_addr(settings.endpoints[i].domain, &sa) < 0) {
            // Name resolution failed, move to next server
            continue;
        }

        // Send "ping" to Server
        time = speedtest_ping(&sa, settings.endpoints[i].port, settings.latency_samples);
        log("%s: server <%s> latency was %ims\n", __func__,
            settings.endpoints[i].domain, time);

        // If this was the lowest latency, record time and index
        if(time >= 0 && time < min_time)
        {
            min_time = time;
            min_time_index = i;
        }

    }

    // No servers were reachable, return error
    if(min_time_index == -1) {
        return -1;
    }

    // lowest latency server we will use for test
    endpoint = &settings.endpoints[min_time_index];

    latency_ms = min_time;
    log("%s: lowest latency server: %s\n", __func__, endpoint->domain);
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
    // Attempt transfer on requested port
    int result = speedtest_transfer("upload", speedtest_upload_worker,
                                    &upload_speed_kbps);

    // Fallback to port 80 on failure
    if(result < 0 && endpoint->port != DEFAULT_PORT) {
        endpoint->port = DEFAULT_PORT;
        result = speedtest_transfer("upload", speedtest_upload_worker,
                                    &upload_speed_kbps);
    }

    return result;
}

// Performs the whole speed test, blocking until complete.
void speedtest_perform(THRD_PARAM_t *p)
{
    int i, num_tests;
    char * test_cmd = p->cptr_val;

    log("%s: unum speedtest " SPEEDTEST_PROTO_VER " cmd: %s\n", __func__, test_cmd);

    reset_results();

    num_tests = config_for_cmd(test_cmd);
    if(num_tests < 0) {
        log("%s: Cannot handle unknown command %s\n", __func__, test_cmd);
        return;
    }

    // Fetch the settings from the Minim API, if possible. If this fails, the
    // defaults defined at the top of this file will be used.
    if(speedtest_fetch_settings(&settings) < 0) {
        settings = speedtest_default_settings();
        log("%s: failed to fetch settings from remote API, "
            "continuing with defaults\n", __func__);
    }

    // Print details of the configured environment.
    speedtest_print_settings();

    if(settings.online) {
        log("%s: streaming results to Minim API as tests finish\n", __func__);
    }

    // Set the global test id if one came with the test settings
    if(settings.test_id != 0) {
        test_id = settings.test_id;
    }

    // Determine the interface to run the test from if running in
    // the AP or MD mode, otherwise keep unspecified
    ifname = NULL;
    if(IS_OPM(UNUM_OPM_AP | UNUM_OPM_MD) &&
       util_get_ipv4_gw_oif(ifname_buf) == 0)
    {
        ifname = ifname_buf;
    }
    if(ifname) {
        log("%s: binding to interface %s for the test\n", __func__, ifname);
    } else {
        log("%s: not binding to specifc interface for the test\n", __func__);
    }

    // ping endpoints
    int ii = 0;
    int samples = 0;
    int sum = 0;
    int count = 3;
    struct sockaddr s_addr;

    for(ii = 0; ii < MAX_ENDPOINTS &&
        settings.ping_endpoints[ii].domain[0] != '\0'; ii++)
    {
        if(util_get_ip4_addr(settings.ping_endpoints[ii].domain, &s_addr) == 0) {
            for(i = 0; i < count; i++) {
                int ret = util_ping(&s_addr,  settings.ping_endpoints[ii].samples);
                if (ret < 0) {
                    continue;
                }
                sum += ret;
                ++samples;
            }
            if(samples <= 0) {
                log("%s: no samples to calculate latency\n", __func__);
            }
            settings.ping_endpoints[ii].latency_ms = (int)(sum/samples);
            log("%s: average latency for %s: %ims\n", __func__,
                settings.ping_endpoints[ii].domain,
                settings.ping_endpoints[ii].latency_ms);
        }
    }

    // Iterate over individual tests, running each one at a time.
    for(i = 0; i < num_tests; ++i) {
        int test_result;
        // Run the test and record result
        test_result = speedtests[i]();
        if(test_result) {
            log("%s: error %d while performing test %d\n", __func__, test_result, i);
            if(i == 0) {
                // if the latency test failed, then there is no endpoint to
                // to perform a speedtest against and we're done
                break;
            }

            log("%s: invalid domain = %s\n", __func__, endpoint->domain);
            endpoint->invalid = 1;

            int all_invalid = 1;
            int j;
            for(j = 0; j < MAX_ENDPOINTS &&
                settings.endpoints[j].domain[0] != '\0'; j++)
            {
                if(settings.endpoints[j].invalid == 0) {
                    all_invalid = 0;
                    break;
                }
            }

            if(!all_invalid) {
                // Restart Test Procedure
                i = -1;
                continue;
            }
            else {
                break;
            }

        }
        // Let other threads have a turn.
        sched_yield();
        // Report intermediate results
        if(i < num_tests - 1) {
            // Stream results except for the last iteration-- the results
            // will be uploaded after the test is over anyway.
            speedtest_upload_results_failfast();
        }
    }

    complete = 1;

    // Deal with the various possible final states.
    if(settings.online == TRUE) {
        if(speedtest_upload_results() < 0) {
            log("%s: results not uploaded to server\n", __func__);
        } else {
            log("%s: results successfully uploaded to server\n", __func__);
        }
    } else {
        log("%s: agent is not activated, not uploading results\n", __func__);
    }

    char results[256];
    speedtest_results_to_json(results, sizeof(results));
    log("%s: results: %s\n", __func__, results);
}

// Starts the requested test type on a new thread.
void cmd_speedtest(char * cmd)
{
    THRD_PARAM_t t_param = {{.cptr_val = cmd}};

    // Perform the test in a new thread so this command can return immediately.
    util_start_thrd("speedtest_perform", speedtest_perform, &t_param, NULL);
}

