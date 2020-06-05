// (c) 2017-2019 minim.co
// unum device provision code

#include "unum.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Set if cert and key files are provisioned for the device.
static UTIL_EVENT_t provision_complete = UTIL_EVENT_INITIALIZER;


// This function blocks the caller till device is provisioned.
// If '-p' option is used the function never blocks.
void wait_for_provision(void)
{
    // Provisioning is disabled
    if(unum_config.no_provision) {
        return;
    }
    // Wait for the event to be set otherwise
    UTIL_EVENT_WAIT(&provision_complete);
}

// This function returns TRUE if the device is provisioned, FALSE otherwise.
// Even if '-p' option is used it still returns TRUE only if provisioned
// (maybe from a previous run while -p was not used).
int is_provisioned(void)
{
    return UTIL_EVENT_IS_SET(&provision_complete);
}

// This function returns TRUE if the device done with provisioning.
// If '-p' option is used it always returns TRUE.
int is_provision_done(void)
{
    if(UTIL_EVENT_IS_SET(&provision_complete) || unum_config.no_provision) {
        return TRUE;
    }
    return FALSE;
}

// If the device is provisioned this function cleans up existent provisioning
// info and terminates the agent with exit code requesting its restart (works
// only w/ monitor process). If the device is not provisioned it has no effect.
// The '-p' option does not affect the behavior of the function.
void destroy_provision_info(void)
{
    if(is_provisioned()) {
        log("%s: removing %s and %s\n", __func__,
            PROVISION_CERT_PNAME, PROVISION_KEY_PNAME);
        unlink(PROVISION_CERT_PNAME);
        unlink(PROVISION_KEY_PNAME);
        util_restart(UNUM_START_REASON_PROVISION);
        // not reacheable
    }
    log("%s: device is not yet provisioned, continuing\n", __func__);
}

// Check if the cert and key files have been provisioned
static int cert_provisioned(void)
{
    struct stat st;
    if(stat(PROVISION_CERT_PNAME, &st) == 0 && st.st_size > 0 &&
       stat(PROVISION_KEY_PNAME, &st) == 0 && st.st_size > 0)
    {
        return TRUE;
    }
    return FALSE;
}

// Save data from memory to a file, returns -1 if fails.
static int save_buf_to_file(char *file, void *buf, int len)
{
    return util_buf_to_file(file, buf, len, 00600);
}

// Set "provisioned" (used for debugging only)
void set_provisioned(void)
{
    UTIL_EVENT_SETALL(&provision_complete);
}

// URL to request device provisioning certs, parameters:
// - the URL prefix
// - MAC addr (in xx:xx:xx:xx:xx:xx format)
#define PROVISION_URL "/v3/unums/%s/certificate"

static void provision(THRD_PARAM_t *p)
{
    unsigned long no_rsp_t = 0;
    unsigned int delay = 0;
    int err, len;
    http_rsp *rsp = NULL;

    log("%s: started\n", __func__);

    log("%s: waiting for commcheck to complete\n", __func__);
    wait_for_conncheck();
    log("%s: done waiting for commcheck\n", __func__);

#if PROVISION_PERIOD_START > 0
    // Random delay at startup to avoid all APs poll server
    // at the same time after large area power outage.
    delay = rand() % PROVISION_PERIOD_START;
    log("%s: delaying first attempt to %d sec\n", __func__, delay);
    sleep(delay);
#endif // PROVISION_PERIOD_START > 0

    util_wd_set_timeout(HTTP_REQ_MAX_TIME + PROVISION_MAX_PERIOD);

    err = -1;
    for(;;) {
        char *my_mac = util_device_mac();
        char *cert_start = NULL;
        char *cert_end = NULL;
        char *key_start = NULL;
        char *key_end = NULL;
        char *sptr;
        char *cert_start_anc = "-----BEGIN CERTIFICATE-----";
        char *cert_end_anc   = "-----END CERTIFICATE-----";
        char *key_start_anc  = "-----BEGIN RSA PRIVATE KEY-----";
        char *key_end_anc    = "-----END RSA PRIVATE KEY-----";
        char url[256];

        for(;;) {
            // Check that we have MAC address
            if(!my_mac) {
                log("%s: cannot get device MAC\n", __func__);
                break;
            }

            // Get the device certificates
            util_build_url(RESOURCE_PROTO_HTTPS, RESOURCE_TYPE_PROVISION,
                           url, sizeof(url), PROVISION_URL, my_mac);

            rsp = http_get(url, NULL);

            if(rsp == NULL)
            {
                log("%s: no response\n", __func__);
                // If no response for over CONNCHECK_OFFLINE_RESTART time period
                // try to restart the conncheck (this will restart the agent)
                if(no_rsp_t == 0) {
                    no_rsp_t = util_time(1);
                } else if(util_time(1) - no_rsp_t > CONNCHECK_OFFLINE_RESTART) {
                    log("%s: restarting conncheck...\n", __func__);
                    restart_conncheck();
                }
                break;
            }
            no_rsp_t = 0;

            if((rsp->code / 100) != 2) {
                log("%s: failed to retrieve certificate for %s\n",
                    __func__, my_mac);
                break;
            }

            // The certificate and the key come together and have to be split.
            // Look for the certs (might be a chain) and key anchor lines first.
            cert_start = strstr(rsp->data, cert_start_anc);
            for(sptr = cert_start; sptr; sptr = strstr(sptr + 1, cert_end_anc))
            {
                cert_end = sptr;
            }
            key_start = strstr(rsp->data, key_start_anc);
            if(key_start && (sptr = strstr(key_start, key_end_anc)) != NULL)
            {
                key_end = sptr;
            }
            if(!cert_start || !cert_end || cert_start >= cert_end) {
                log("%s: cannot extract client certificate\n", __func__);
                break;
            }
            if(!key_start || !key_end || key_start >= key_end) {
                log("%s: cannot extract private key\n", __func__);
                break;
            }

            // Save the client certificate and key
            len = (cert_end - cert_start) + strlen(cert_end_anc);
            if(save_buf_to_file(PROVISION_CERT_PNAME, cert_start, len) != 0) {
                break;
            }
            len = (key_end - key_start) + strlen(key_end_anc);
            if(save_buf_to_file(PROVISION_KEY_PNAME, key_start, len) != 0) {
                break;
            }

            // Success
            err = 0;
            break;
        }

        if(rsp) {
            free_rsp(rsp);
            rsp = NULL;
        }

        util_wd_poll();

        if(!err) {
            log("%s: successfully provisioned %s\n",
                __func__, my_mac);
            UTIL_EVENT_SETALL(&provision_complete);
            break;
        }

        // Cleanup
        unlink(PROVISION_CERT_PNAME);
        unlink(PROVISION_KEY_PNAME);

        // Calculate delay delay before next provisioning attempt
        if(delay < PROVISION_MAX_PERIOD) {
            delay += rand() % PROVISION_PERIOD_INC;
        }
        if(delay > PROVISION_MAX_PERIOD) {
            delay = PROVISION_MAX_PERIOD;
        }
        log("%s: error, retrying in %d sec\n", __func__, delay);
        sleep(delay);
    }

    log("%s: done\n", __func__);
}

// Subsystem init fuction
int provision_init(int level)
{
    int ret = 0;
    if(level == INIT_LEVEL_PROVISION) {
        // Start the device provisioning job if not yet provisioned and
        // there was no command line option to turn it off.
        if(cert_provisioned()) {
            log("%s: already done, skipping\n", __func__);
            UTIL_EVENT_SETALL(&provision_complete);
        } else if(unum_config.no_provision) {
            log("%s: disabled, skipping\n", __func__);
        } else {
            ret = util_start_thrd("provision", provision, NULL, NULL);
        }
    }
    return ret;
}
