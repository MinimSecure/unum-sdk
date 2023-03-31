// (c) 2022 minim.co
// ubus unum device telemetry code

#include "unum.h"

#ifdef FEATURE_UBUS_TELEMETRY
static struct ubus_context *ctx = NULL;

#ifdef FEATURE_IPV6_TELEMETRY

static DEV_IPV6_CFG_t ipv6_prefixes[MAX_IPV6_ADDRESSES_PER_MAC];

static void wan6_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    if (!msg)
        return;
    struct blob_attr *pos0;
    size_t rem0 = blobmsg_data_len(msg);
    unsigned prefix_idx = 0;

    memset(ipv6_prefixes, '\0', sizeof(ipv6_prefixes));

    __blob_for_each_attr(pos0, blobmsg_data(msg), rem0) {
        const struct blobmsg_hdr *hdr0 = (struct blobmsg_hdr *) blob_data(pos0);
        const char* name0 = (const char*) hdr0->name;
        uint16_t namelen0 = hdr0->namelen;
        if (!strncmp(name0, "ipv6-prefix", namelen0)) {
            struct blob_attr* pos1;
            size_t rem1 = blobmsg_data_len(pos0);
            __blob_for_each_attr(pos1, blobmsg_data(pos0), rem1) {
                // per prefix
                struct blob_attr* pos2;
                size_t rem2 = blobmsg_data_len(pos1);
                __blob_for_each_attr(pos2, blobmsg_data(pos1), rem2) {
                    // per prefix data element
                    const struct blobmsg_hdr *hdr2 = (struct blobmsg_hdr *) blob_data(pos2);
                    const char* name2 = (const char*) hdr2->name;
                    uint16_t namelen2 = hdr2->namelen;
                    if (!strncmp(name2, "address", namelen2) && blob_id(pos2) == BLOBMSG_TYPE_STRING) {
                        inet_pton(AF_INET6, blobmsg_data(pos2), ipv6_prefixes[prefix_idx].addr.b);
                    } else if (!strncmp(name2, "mask", namelen2) && blob_id(pos2) == BLOBMSG_TYPE_INT32) {
                        uint32_t prefix_len = be32_to_cpu(*(uint32_t *)blobmsg_data(pos2));
                        ipv6_prefixes[prefix_idx].prefix_len = prefix_len;
                    } else if (!strncmp(name2, "preferred", namelen2) && blob_id(pos2) == BLOBMSG_TYPE_INT32) {
                        uint32_t ifa_preferred = be32_to_cpu(*(uint32_t *)blobmsg_data(pos2));
                        ipv6_prefixes[prefix_idx].ifa_preferred = ifa_preferred;
                    } else if (!strncmp(name2, "valid", namelen2) && blob_id(pos2) == BLOBMSG_TYPE_INT32) {
                        uint32_t ifa_valid = be32_to_cpu(*(uint32_t *)blobmsg_data(pos2));
                        ipv6_prefixes[prefix_idx].ifa_valid = ifa_valid;
                    }
                }
                if (++prefix_idx == MAX_IPV6_ADDRESSES_PER_MAC) {
                    break;
                }
            }
        }
        if (prefix_idx == MAX_IPV6_ADDRESSES_PER_MAC) {
            break;
        }
    }
}

// Return a pointer to the table of prefixes - contains up to
// MAX_IPV6_ADDRESSES_PER_MAC entries, invalid entries are all zeroes.
const DEV_IPV6_CFG_t* telemetry_ubus_get_ipv6_prefixes(void) {
    return ipv6_prefixes;
}

#endif // FEATURE_IPV6_TELEMETRY

#ifdef FEATURE_SUPPORTS_SAMBA
static JSON_VAL_FARRAY_t __smb_fa_ptr = NULL;

static SMBT_JSON_TPL_DEVICE_STATE_t smbt_device_state;

// Initialize last data to non-zero to force a difference detection at boot
static SMBT_JSON_TPL_DEVICE_STATE_t last_smbt_device_state = { {}, ~0UL, ~0UL };

// smb/usb device template
static JSON_OBJ_TPL_t tpl_tbl_devices_obj = {
   { "id", { .type = JSON_VAL_STR, {.s = smbt_device_state.id}}},
   { "bytes_free", { .type = JSON_VAL_PUL, {.pul = &smbt_device_state.bytes_free}}},
   { "bytes_total", { .type = JSON_VAL_PUL, {.pul = &smbt_device_state.bytes_total}}},
   { NULL }
};

static JSON_VAL_TPL_t tpl_tbl_devices_obj_val = {
   .type = JSON_VAL_OBJ, { .o = tpl_tbl_devices_obj }
};

// Dynamically builds JSON template for the smb/usb telemetry
// devices array.
JSON_VAL_TPL_t *smbt_tpl_devices_array_f(char *key, int ii)
{
    if(ii == 0) {
        return smbt_device_state.id[0] == 0 ? NULL : &tpl_tbl_devices_obj_val;
    } else {
        return NULL;
    }
}

static void block_info_cb(struct ubus_request *req, int type, struct blob_attr *msg)
{
    struct blob_attr *pos0, *pos1, *pos2;
    size_t rem0, rem1, rem2;
    int ret;
    int mount_count = 0;
    char * uuid = NULL;
    char * mount = NULL;
    const char * name = NULL;
    const char * base_path = "/mnt/sd";
    struct statvfs buf;

    if (!msg)
        return;

    rem0 = blobmsg_data_len(msg);
    __blob_for_each_attr(pos0, blobmsg_data(msg), rem0) {
        if (!strcmp(blobmsg_name(pos0), "devices")) {
           rem1 = blobmsg_data_len(pos0);
            __blob_for_each_attr(pos1, blobmsg_data(pos0), rem1) {
                char * _uuid = NULL;
                char * _mount = NULL;
                rem2 = blobmsg_data_len(pos1);
                __blob_for_each_attr(pos2, blobmsg_data(pos1), rem2) {
                    if (blob_id(pos2) == BLOBMSG_TYPE_STRING) {
                        name = blobmsg_name(pos2);
                        if (!strcmp(name, "uuid")) {
                            _uuid = blobmsg_get_string(pos2);
                        } else if (!strcmp(name, "mount")) {
                            _mount = blobmsg_get_string(pos2);
                        }
                    }
                }
                if(_mount != NULL && !strncmp(base_path, _mount, strlen(base_path))) {
                    mount_count++;
                    if(mount_count == 1) {
                        uuid = _uuid;
                        mount = _mount;
                    }
		}
            }
        }
    }

    if(mount_count == 1) {
        ret = statvfs(mount, &buf);
        if(ret < 0) {
            log("%s: statvfs failed %d\n", __func__, ret);
        } else {
            if (uuid != NULL) {
                strncpy(smbt_device_state.id, uuid, sizeof(smbt_device_state.id));
                smbt_device_state.id[sizeof(smbt_device_state.id) - 1] = 0;
                smbt_device_state.bytes_free = (buf.f_bavail * buf.f_bsize);
                smbt_device_state.bytes_total = (buf.f_blocks * buf.f_bsize);
	    } else {
                memset(&smbt_device_state, 0, sizeof(smbt_device_state));
            }
        }
    } else {
        memset(&smbt_device_state, 0, sizeof(smbt_device_state));
    }

    if(!memcmp(&smbt_device_state, &last_smbt_device_state, sizeof(smbt_device_state))) {
        __smb_fa_ptr = NULL;
    } else {
        __smb_fa_ptr = smbt_tpl_devices_array_f;
    }
    memcpy(&last_smbt_device_state, &smbt_device_state, sizeof(last_smbt_device_state));
}

JSON_VAL_FARRAY_t telemetry_ubus_get_smb_fa_ptr(void) {
    return __smb_fa_ptr;
}

void telemetry_ubus_refresh_smb(void)
{
    const char block_string[] = "block";
    uint32_t   block_id = 0;
    static struct blob_buf b;
    int result;

    result = ubus_lookup_id(ctx, block_string, &block_id);
    if (result != 0) {
        log("%s: Failed to ubus %s result=%d\n", __func__, block_string, result);
        return;
    }
    blob_buf_init(&b, 0);
    ubus_invoke(ctx, block_id, "info", b.head, block_info_cb, NULL, 30000);
}
#endif // FEATURE_SUPPORTS_SAMBA

void telemetry_ubus_refresh(void)
{
#ifdef FEATURE_IPV6_TELEMETRY
    const char wan6_string[] = "network.interface.wan6";
    uint32_t   wan6_id = 0;
    int result = ubus_lookup_id(ctx, wan6_string, &wan6_id);
    if (result != 0) {
        log("%s: Failed to ubus %s result=%d\n", __func__, wan6_string, result);
        return;
    }
    static struct blob_buf b;
    blob_buf_init(&b, 0);
    ubus_invoke(ctx, wan6_id, "status", b.head, wan6_cb, NULL, 30000);
#endif // FEATURE_IPV6_TELEMETRY

#ifdef FEATURE_SUPPORTS_SAMBA
    telemetry_ubus_refresh_smb();
#endif // FEATURE_SUPPORTS_SAMBA
}

// initialise the ubus telemetry
void telemetry_ubus_init(void)
{
    ctx = ubus_connect(NULL);
    if (!ctx) {
        log("%s: failed to connect to ubus\n", __func__);
        return;
    }

    telemetry_ubus_refresh();
}
#endif // FEATURE_UBUS_TELEMETRY
