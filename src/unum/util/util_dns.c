// Copyright 2019 - 2020 Minim Inc
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

#include "unum.h"
#include "dns.h"

// Internal Constants
#define EXPECTED_TOKENS_PER_ENTRY 4
#define MAX_TXT_RECORD_LENGTH     1024
#define TXT_RECORD_SEPARATOR      ";"
#define TXT_TOKEN_SEPARATOR       ":"
#define HASH_MAX_LEN              24
#define MINIM_NS_URL_FMT          "%s.v1.ns.minim.co"
#define DOMAIN_SEPARATOR          '.'
#define DNS_UTIL_SUCCESS           0
#define DNS_UTIL_ERR              -1
#define DNS_UTIL_ERR_WHITELIST    -2
#define DNS_UTIL_ERR_TOKEN_COUNT  -3
#define DNS_UTIL_ERR_PUSH         -4
#define DNS_UTIL_ERR_SOCKET       -5
#define DNS_UTIL_ERR_LIB          -6
#define DNS_UTIL_ERR_TIMEOUT      -7

char servers[RESOURCE_TYPE_MAX][RESOURCE_URL_LEN];
char dns_entries[DNS_ENTRIES_MAX + 1][DNS_ENTRY_MAX_LENGTH];

// List of DNS Servers To Query
static char *dns_servers[] = { "8.8.8.8", "1.1.1.1", NULL };

// If DNS Queries Fail, Use this Hardcoded TXT Record
static void util_get_txt_record_static(char *txt, int len)
{
    strncpy(txt,
            "api:api.minim.co:80:34.207.26.129;"
            "api:api.minim.co:443:34.207.26.129;"
            "my:my.minim.co:443:34.207.26.129;"
            "releases:releases.minim.co:443:34.207.26.129;"
            "provision:provision.minim.co:443:34.207.26.129;",
            len);
    txt[len - 1] = '\0';
}

// Server Whitelist from Header
static char *api_server_whitelist[] = API_SERVER_WHITELIST;

/***
 * Calculate SHA256 Hash For Supplied Buffer
 *
 * @param data - Buffer to hash
 * @param data_len - Length of buffer to hash
 * @param result - Hash result, MUST be at least 32 bytes
 */
#ifdef USE_OPEN_SSL
void calculate_sha256(char *data, int data_len, unsigned char *result)
{
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, data_len);
    SHA256_Final(result, &sha256);
}
#else
void calculate_sha256(char *data, int data_len, unsigned char *result)
{
    mbedtls_sha256_ret((unsigned char*) data, data_len, result, 0);
}
#endif

/***
 * Evaluates if a TXT Record is valid.
 *
 * @param txt - Buffer w/ TXT Record
 * @return - int DNS_UTIL_SUCCESS on success
 *               DNS_UTIL_FAILURE on failure
 */
static int util_txt_record_is_valid(char *txt)
{
    char str[MAX_TXT_RECORD_LENGTH];

    // Copy TXT So It Can Be Tokenized
    strncpy(str, txt, sizeof(str));
    str[sizeof(str) - 1] = '\0';

    // Get First Record in TXT
    char *next_record;
    char *record = strtok_r(str, TXT_RECORD_SEPARATOR, &next_record);

    // Process Records
    while(record != NULL)
    {
        // Get First Token in Record
        char *next_token;
        char *token = strtok_r(record, TXT_TOKEN_SEPARATOR, &next_token);
        int token_index = 0;
        int matches = 0;
        // Process Tokens
        while(token != NULL)
        {
            if(token_index == 1)
            {
                int second_to_last_dot = 0;
                int last_dot = 0;
                char *pch = strchr(token, DOMAIN_SEPARATOR);
                while(pch != NULL)
                {
                    second_to_last_dot = last_dot;
                    last_dot = pch - token + 1;
                    pch = strchr(pch + 1, DOMAIN_SEPARATOR);
                }

                int i;
                for(i = 0; api_server_whitelist[i] != NULL; i++)
                {
                    if(0 == strcmp(api_server_whitelist[i],
                                    &token[second_to_last_dot]))
                    {
                        matches++;
                        break;
                    }
                }
            }
            else if(token_index > EXPECTED_TOKENS_PER_ENTRY)
            {
                log("%s: Too many tokens in DNS entry\n", __FUNCTION__);
                return DNS_UTIL_ERR_TOKEN_COUNT;
            }

            // advance to next token in record
            token = strtok_r(NULL, TXT_TOKEN_SEPARATOR, &next_token);
            token_index++;
        }

        if(token_index != EXPECTED_TOKENS_PER_ENTRY)
        {
            log("%s: Not enough tokens in DNS entry\n", __FUNCTION__);
            return DNS_UTIL_ERR_TOKEN_COUNT;
        }
        else if(matches == 0)
        {
            log("%s: An entry did not match the whitelist\n", __FUNCTION__);
            return DNS_UTIL_ERR_WHITELIST;
        }

        // advance to next record in txt
        record = strtok_r(NULL, TXT_RECORD_SEPARATOR, &next_record);
    }

    return DNS_UTIL_SUCCESS;
}

/***
 * Get Device ID Hash
 *
 * @return - char* Truncated Device ID Hash
 */
static char* get_device_id_hash()
{
    char *device_id;
    static char hash_result_string[65] = "";

    // If enabled, first time through build string
    if((!hash_result_string[0]) && ((device_id = util_device_mac()) != NULL))
    {
        // Perform Hashing
        unsigned char hash_result_bytes[32];
        calculate_sha256(device_id, strlen(device_id), hash_result_bytes);

        // Put hashed value into string
        int i;
        for(i = 0; i < sizeof(hash_result_bytes); i++) {
            sprintf(&hash_result_string[2 * i], "%02x", hash_result_bytes[i]);
        }

        // Truncate String
        hash_result_string[HASH_MAX_LEN] = '\0';
    }

    return hash_result_string;
}

/***
 * Get DNS TXT Record From DNS Packet
 *
 * @param pkt - filled packet returned from dns library
 * @param buf - buffer to put TXT record contents into
 * @param buf_len - length of buffer to prevent overrun
 */
static void dns_get_txt(struct dns_packet *pkt, char *buf, int buf_len)
{

    enum dns_section section;
    struct dns_rr rr;
    int error;
    struct dns_rr_i *I = dns_rr_i_new(pkt, .section = 0);
    union dns_any any;

    section = 0;
    while(dns_rr_grep(&rr, 1, I, pkt, &error))
    {
        if(section != rr.section && rr.section == DNS_S_AN
                && rr.type == DNS_T_TXT)
        {
            dns_any_parse(dns_any_init(&any, sizeof any), &rr, pkt);
            dns_any_print(buf, buf_len, &any, rr.type);
            break;
        }
        section = rr.section;
    }
    memmove(buf, &buf[1], strlen(buf));
    buf[strlen(buf) - 1] = '\0';
}

/***
 * Send DNS Query
 *
 * @param ns - IP Address (string) of nameserver to query
 * @param domain_name - The domain name to send in the query
 * @param txt - The buffer to put TXT record into
 * @param txt_len - The length of the buffer
 */
static int send_query(const char *ns, const char *domain_name, char *txt,
        int txt_len)
{

    struct dns_packet *A, *Q = dns_p_new(512);
    struct sockaddr_in sa, ss;
    struct dns_socket *so;
    const int dns_port = 53;
    int error;

    // Setup Sockets
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = INADDR_ANY;
    sa.sin_port = 0;

    ss.sin_family = AF_INET;
    inet_pton(AF_INET, ns, &(ss.sin_addr));
    ss.sin_port = htons(dns_port);

    if((error = dns_p_push(Q, DNS_S_QD, domain_name, strlen(domain_name),
            DNS_T_TXT, DNS_C_IN, 0, 0)))
    {
        log("%s: Failed to push DNS request, error=%d\n", __FUNCTION__, error);
        return DNS_UTIL_ERR_PUSH;
    }

    // Set Recursion Desired Flag
    dns_header(Q)->rd = 1;

    if(!(so = dns_so_open((struct sockaddr*) &sa, SOCK_DGRAM, dns_opts(),
            &error)))
    {
        log("%s: Failed to open DNS socket, error=%d\n", __FUNCTION__, error);
        return DNS_UTIL_ERR_SOCKET;
    }

    // Send Query and Poll For Response
    while(!(A = dns_so_query(so, Q, (struct sockaddr*) &ss, &error)))
    {
        if(error != EAGAIN)
        {
            log("%s: Error while polling for response, error=%d\n",
                __FUNCTION__, error);
            return DNS_UTIL_ERR_LIB;
        }
        if(dns_so_elapsed(so) > unum_config.dns_timeout)
        {
            log("%s: Timed out after %d seconds\n", __FUNCTION__,
                unum_config.dns_timeout);
            return DNS_UTIL_ERR_TIMEOUT;
        }

        dns_so_poll(so, 1);
    }

    // Get TXT Record From Response
    dns_get_txt(A, txt, txt_len);

    dns_so_close(so);

    return DNS_UTIL_SUCCESS;
}

/***
 * Get TXT Record
 *
 * @param txt - Character buffer to fill in with TXT data
 * @param buflen - Length of character buffer to prevent overrun
 * @return  Zero on success, -1 on error. 
 */
static int util_get_txt_record(char *txt, int buflen)
{
    char domain[128];
    int i;
    int conn_err = 1;

    // Get Domain Name To Lookup
    char *device_id = get_device_id_hash();
    snprintf(domain, sizeof(domain), MINIM_NS_URL_FMT, device_id);

    while(conn_err != 0)
    {
        // Iterate Over Names
        conn_err = 0;
        for(i = 0; dns_servers[i] != NULL; i++)
        {
            log_dbg("%s: Sending TXT record query to %s\n", __FUNCTION__, dns_servers[i]);
            int status = send_query(dns_servers[i], domain, txt, buflen);
            if(DNS_UTIL_SUCCESS == status)
            {
                log_dbg("%s: Received response from server, validating...\n", __FUNCTION__);
                if(DNS_UTIL_SUCCESS == util_txt_record_is_valid(txt))
                {
                    log("%s: TXT record from %s is valid. DONE.\n",
                            __FUNCTION__, dns_servers[i]);
                    return 0;
                }
            } else {
                conn_err++;
            }
            util_wd_poll();
        }

        if(conn_err) {
            return -1;
        }
    }

    // If we made it here, then our lookups failed
    // use hardcoded TXT record
    log("%s: Falling back to static TXT record. DONE\n", __FUNCTION__);
    util_get_txt_record_static(txt, buflen);
    return 0;
}

/***
 * Update DNS Mappings
 *
 * @param txt - String containing TXT Record
 */
void update_dns_mappings_from_txt_record(char *txt)
{
    // Copy TXT So It Can be Tokenized
    char str[MAX_TXT_RECORD_LENGTH];
    strncpy(str, txt, sizeof(str));
    str[sizeof(str) - 1] = '\0';

    // Get First Record in TXT
    char *next_record;
    char *record = strtok_r(str, TXT_RECORD_SEPARATOR, &next_record);

    int entry_index = 0;

    // Process Records
    while(record != NULL && entry_index < DNS_ENTRIES_MAX)
    {
        //  Find First : In Record
        char *dns_entry = strchr(record, ':');
        dns_entry++;

        // Copy Everything After First : To dns_entries
        strncpy(dns_entries[entry_index], dns_entry, DNS_ENTRY_MAX_LENGTH);
        dns_entries[entry_index][DNS_ENTRY_MAX_LENGTH - 1] = '\0';
        ++entry_index;

        // Advance to Next Record
        record = strtok_r(NULL, TXT_RECORD_SEPARATOR, &next_record);
    }

    // Mark End of List
    dns_entries[entry_index][0] = '\0';

    // Print out dns_entries
    int i;
    for(i = 0; i < entry_index; i++) {
        log("%s: dns entry %d = %s\n", __FUNCTION__, i, dns_entries[i]);
    }

}

/***
 * Update API URL Map
 *
 * @param txt - String containing TXT Record
 */
void update_api_url_map_from_txt_record(char *txt)
{
    char str[MAX_TXT_RECORD_LENGTH];

    // Copy TXT So It Can Be Tokenized
    strncpy(str, txt, sizeof(str));
    str[sizeof(str) - 1] = '\0';

    // Get First Record in TXT
    char *next_record;
    char *record = strtok_r(str, TXT_RECORD_SEPARATOR, &next_record);

    // Process Records
    while(record != NULL)
    {
        // Get First Token in Record
        char *next_token;
        char *token = strtok_r(record, TXT_TOKEN_SEPARATOR, &next_token);
        int token_index = 0;
        char *record_key = 0;

        // Process Tokens
        while(token != NULL)
        {
            // Save first token, this is the record key
            if(token_index == 0)
            {
                record_key = token;
            } else if(token_index == 1)
            {
                // 2nd token is the URL, save it off based on key
                if(0 == strcmp(RESOURCE_TYPE_API_STR, record_key))
                {
                    strncpy(servers[RESOURCE_TYPE_API], token,
                            RESOURCE_URL_LEN);
                    servers[RESOURCE_TYPE_API][RESOURCE_URL_LEN - 1] = '\0';

                } else if(0 == strcmp(RESOURCE_TYPE_MY_STR, record_key))
                {
                    strncpy(servers[RESOURCE_TYPE_MY], token,
                            RESOURCE_URL_LEN);
                    servers[RESOURCE_TYPE_MY][RESOURCE_URL_LEN - 1] = '\0';

                } else if(0 == strcmp(RESOURCE_TYPE_RELEASES_STR, record_key))
                {
                    strncpy(servers[RESOURCE_TYPE_RELEASES], token,
                            RESOURCE_URL_LEN);
                    servers[RESOURCE_TYPE_RELEASES][RESOURCE_URL_LEN - 1] = '\0';

                } else if(0 == strcmp(RESOURCE_TYPE_PROVISION_STR, record_key))
                {
                    strncpy(servers[RESOURCE_TYPE_PROVISION], token,
                            RESOURCE_URL_LEN);
                    servers[RESOURCE_TYPE_PROVISION][RESOURCE_URL_LEN - 1] = '\0';
                }
            }

            // advance to next token in record
            token = strtok_r(NULL, TXT_TOKEN_SEPARATOR, &next_token);
            token_index++;
        }

        // advance to next record in txt
        record = strtok_r(NULL, TXT_RECORD_SEPARATOR, &next_record);
    }

    // print out all the servers that were found
    int i;
    for(i = 0; i < RESOURCE_TYPE_MAX; i++)
    {
        log("%s: server for resource %d = %s\n", __FUNCTION__, i, servers[i]);
    }
}

// Get Servers
//
// This function generates a DNS query based on this device's unique ID.
// The response is parsed and device specific URL information is loaded
// into a server map for subsequent usage in HTTP requests.
//
int util_get_servers(void)
{
    int result;
    char txt_record[1024];

    // Get TXT Record
    log("%s: Retrieving TXT record...\n", __FUNCTION__);
    result = util_get_txt_record(txt_record, sizeof(txt_record));
    log("%s: DONE, result=%d\n", __FUNCTION__, result);

    if (result) {
        log("%s: failed to get dns!\n", __func__);
        return -1;
    }

    // Parse TXT record and update URL Mappings
    log("%s: Updating URL map from TXT record...\n", __FUNCTION__);
    update_api_url_map_from_txt_record(txt_record);
    log("%s: DONE\n", __FUNCTION__);

    // Parse TXT Record and Update Static DNS Mapping List (used by libcurl)
    log("%s: Updating libcurl DNS entries from TXT record...\n", __FUNCTION__);
    update_dns_mappings_from_txt_record(txt_record);
    log("%s: DONE\n", __FUNCTION__);

    return 0;
}

#ifdef DEBUG

/***
 * Returns a malformed TXT Record for Testing
 *
 * This TXT record has an invalid first entry, it is missing the "api:" in
 * front of the entry.
 *
 */
static void util_get_txt_record_static_test1(char *txt, int len)
{
    strncpy(txt,
    "api.minim.co:80:34.207.26.129;"
    "api:api.minim.co:443:34.207.26.129;"
    "my:my.minim.co:443:34.207.26.129;"
    "releases:releases.minim.co:443:34.207.26.129;"
    "provision:provision.minim.co:443:34.207.26.129;",
    len);
    txt[len - 1] = '\0';
}

/***
 * Returns a malformed TXT Record for Testing
 *
 * This TXT record has an invalid second entry, it is missing the "minim:" in
 *  front of the entry.
 *
 */
static void util_get_txt_record_static_test2(char *txt, int len)
{
    strncpy(txt,
    "api:api.minim.co:80:34.207.26.129;"
    "minim.co:443:34.207.26.129;"
    "my:my.minim.co:443:34.207.26.129;"
    "releases:releases.minim.co:443:34.207.26.129;"
    "provision:provision.minim.co:443:34.207.26.129;",
    len);
    txt[len - 1] = '\0';
}

/***
 * Returns a malformed TXT Record for Testing
 *
 * This TXT record is missing the newline separator between records
 *
 */
static void util_get_txt_record_static_test3(char *txt, int len)
{
    strncpy(txt,
    "api:api.minim.co:80:34.207.26.129\n"
    "minim.co:443:34.207.26.129\n"
    "my:my.minim.co:443:34.207.26.129\n"
    "releases:releases.minim.co:443:34.207.26.129\n"
    "provision:provision.minim.co:443:34.207.26.129\n",
    len);
    txt[len - 1] = '\0';
}

/***
 * Returns a malformed TXT Record for Testing
 *
 * This TXT record has an address not on the whitelist
 *
 */
static void util_get_txt_record_static_test4(char *txt, int len)
{
    strncpy(txt,
            "api:api.google.com:80:34.207.26.129;"
            "api:api.minim.co:443:34.207.26.129;"
            "my:my.minim.co:443:34.207.26.129;"
            "releases:releases.minim.co:443:34.207.26.129;"
            "provision:provision.minim.co:443:34.207.26.129;",
            len);
    txt[len - 1] = '\0';
}

/***
 * Test Hash
 *
 * Gets device ID and computes hash and truncates to correct length
 * for retrieving server list.
 *
 */
static void test_hash()
{
    printf("Testing Hash Function...\n\n");
    printf("\tDevice Id: %s\n", util_device_mac());
    printf("\tHash: %s\n", get_device_id_hash());

    return;
}

/***
 * Test TXT Get
 *
 * Query for TXT Record Via DNS
 *
 */
static void test_txt_get()
{
    char string[1024];

    // Get TXT Record via DNS Query and Display
    printf("Testing TXT Record Retrieval Function...\n\n");
    util_get_txt_record(string, sizeof(string));
    printf("TXT Record:\n");
    printf("%s\n", string);

    return;
}

/***
 * Test TXT is Valid
 *
 * Run validity check on several malformed TXT records
 */
static void test_txt_is_valid()
{
    char txt[1024];
    int result;

    // Get Record and Validate -- Should be Valid
    memset(txt, 0, 1024);
    util_get_txt_record_static(txt, sizeof(txt));
    printf("*************************************\n");
    printf("txt record:\n%s\n", txt);
    result = util_txt_record_is_valid(txt);
    result == 0 ? printf("TXT Record Is Valid\n") :
                  printf("TXT Record is Invalid\n");

    // Get Test Record and Validate -- Should be Invalid
    memset(txt, 0, 1024);
    util_get_txt_record_static_test1(txt, sizeof(txt));
    printf("*************************************\n");
    printf("test1 txt record:\n%s\n", txt);
    result = util_txt_record_is_valid(txt);
    result == 0 ? printf("TXT Record Is Valid\n") :
                  printf("TXT Record is Invalid\n");

    // Get Test Record and Validate -- Should be Invalid
    memset(txt, 0, 1024);
    util_get_txt_record_static_test2(txt, sizeof(txt));
    printf("*************************************\n");
    printf("test2 txt record:\n%s\n", txt);
    result = util_txt_record_is_valid(txt);
    result == 0 ? printf("TXT Record Is Valid\n") :
                  printf("TXT Record is Invalid\n");

    // Get Test Record and Validate -- Should be Invalid
    memset(txt, 0, 1024);
    util_get_txt_record_static_test3(txt, sizeof(txt));
    printf("*************************************\n");
    printf("test3 txt record:\n%s\n", txt);
    result = util_txt_record_is_valid(txt);
    result == 0 ? printf("TXT Record Is Valid\n") :
                  printf("TXT Record is Invalid\n");

    // Get Test Record and Validate -- Should be Invalid
    memset(txt, 0, 1024);
    util_get_txt_record_static_test4(txt, sizeof(txt));
    printf("*************************************\n");
    printf("test4 txt record:\n%s\n", txt);
    result = util_txt_record_is_valid(txt);
    result == 0 ? printf("TXT Record Is Valid\n") :
                  printf("TXT Record is Invalid\n");

    return;
}

/***
 * Test Update DNS
 *
 * Uses the hardcoded TXT record to update the DNS entries typically used by
 * libcurl
 */
static void test_update_dns()
{
    // Print Out List of IP/Port/Name Mappings
    printf("DNS Entries (PRE UPDATE):\n");
    int i=0;
    for(i=0; i<DNS_ENTRIES_MAX; i++) {
        printf("DNS Entry [%02d]: %s\n", i, dns_entries[i]);
    }

    // Fetch Hardcoded TXT Record
    printf("Test TXT Record Content:\n");
    char txt_record[1024];
    util_get_txt_record_static(txt_record, sizeof(txt_record));
    printf("%s\n", txt_record);
    printf("\n");

    // Update Ip/Port/Name Mappings
    printf("Updating DNS Entries From TXT Record...\n");
    update_dns_mappings_from_txt_record(txt_record);
    printf("...DONE\n\n");

    // Print Out List of IP/Port/Name Mappings Again -- Should be Updated
    printf("\n");
    printf("DNS Entries (POST UPDATE):\n");
    for(i=0; i<DNS_ENTRIES_MAX; i++) {
        printf("DNS Entry [%02d]: %s\n", i, dns_entries[i]);
    }

    // Count entries in the DNS name list
    printf("Counting entries...\n");
    int ii;
    for(ii = 0; dns_entries[ii][0] != '\0'; ++ii);
    printf("Entry count: %d\n", ii);

    char *name;
    for(ii = 0, name = "support.minim.co"; name[0] != '\0';
        name = dns_entries[ii++])
    {
        printf("Name is: %s\n", name);
    }



    return;
}

/***
 * Test Update URL Map
 *
 * Uses the hardcoded TXT record to update the URL map used by various telemetry
 * senders
 */
static void test_update_api_url_map()
{
    // Print Out API URL Map
    printf("API MAP (PRE UPDATE):\n");
    printf("\tAPI Server: %s\n", servers[RESOURCE_TYPE_API]);
    printf("\tMY Server: %s\n", servers[RESOURCE_TYPE_MY]);
    printf("\tRELEASES Server: %s\n", servers[RESOURCE_TYPE_RELEASES]);
    printf("\tPROVISION Server: %s\n", servers[RESOURCE_TYPE_PROVISION]);
    printf("\n");

    // Fetch Hardcoded TXT Record
    printf("Test TXT Record Content:\n");
    char txt_record[1024];
    util_get_txt_record_static(txt_record, sizeof(txt_record));
    printf("%s\n", txt_record);
    printf("\n");

    // Update API URL Map
    printf("Updating Servers From TXT Record...\n");
    update_api_url_map_from_txt_record(txt_record);
    printf("...DONE\n\n");

    // Print Out Map Again -- Should Be Updated
    printf("API MAP (POST UPDATE):\n");
    printf("\tAPI Server: %s\n", servers[RESOURCE_TYPE_API]);
    printf("\tMY Server: %s\n", servers[RESOURCE_TYPE_MY]);
    printf("\tRELEASES Server: %s\n", servers[RESOURCE_TYPE_RELEASES]);
    printf("\tPROVISION Server: %s\n", servers[RESOURCE_TYPE_PROVISION]);


    return;
}

/***
 * Test Atomic Increment / Decrement
 */
static void util_test_atomic()
{
#ifdef DNS_ATOMIC_FETCH_ADD
    unsigned long atomc_int = 0;
    printf("atomc_int = %lu\n", atomc_int);
    DNS_ATOMIC_FETCH_ADD(&atomc_int);
    printf("after increment atomc_int = %lu\n", atomc_int);
    DNS_ATOMIC_FETCH_SUB(&atomc_int);
    printf("after decrement atomc_int = %lu\n", atomc_int);
#else
    printf("not supported on this platform\n");
#endif
}

/***
 * Test DNS Subsystem
 *
 * This is the entry point for DNS subsystem tests.
 */
int test_dns()
{
    while(1)
    {
        char cmd_buffer[32];

        printf("DNS Test Menu\n");
        printf("\ta) Generate and display hash from device ID\n");
        printf("\tb) Get TXT Record via DNS\n");
        printf("\tc) Validate canned TXT Records\n");
        printf("\td) Use default TXT record to update libcurl DNS entries\n");
        printf("\te) Use default TXT record to update API map\n");
        printf("\tf) Get DNS Servers (main entry pt)\n");
        printf("\tg) Test Atomic Increment / Decrement Functions\n");
        printf("\tx) EXIT\n");
        printf("Choice:\n");

        fgets(cmd_buffer, sizeof(cmd_buffer), stdin);

        switch(cmd_buffer[0])
        {
        case 'a':
            test_hash();
            break;
        case 'b':
            test_txt_get();
            break;
        case 'c':
            test_txt_is_valid();
            break;
        case 'd':
            test_update_dns();
            break;
        case 'e':
            test_update_api_url_map();
            break;
        case 'f':
            util_get_servers();
            break;
        case 'g':
            util_test_atomic();
            break;
        case 'x':
            return 0;
        default:
            break;
        }
    }
}

#endif
