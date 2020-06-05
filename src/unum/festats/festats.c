// (c) 2018 minim.co
// forwarding engine stats gathering subsystem

#include "unum.h"
#include "festats_platform.h"


/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// Debug macros for tests
#ifdef DEBUG
#  define DBG_ARP(args...) ((get_test_num() == U_TEST_FE_ARP) ? \
                            log_dbg(args) : 0)
#  define DBG_DFG(args...) ((get_test_num() == U_TEST_FE_DEFRAG) ? \
                            log_dbg(args) : 0)
#else // DEBUG
#  define DBG_ARP(args...) /* Nothiing */
#  define DBG_DFG(args...) /* Nothiing */
#endif // DEBUG


// Table of connections
// This is allocated at startup and never released
static FE_CONN_t **conn_tbl;
// Connection table entry present flag. This is split into separate array
// to allow clearing it fast. 
static unsigned char *conn_present;

// ARP table, allocated at startup and never released
static FE_ARP_t *arp_tbl;

// Working copy of festats connection table stats struct, only festats
// thread writes into this table.
static FE_TABLE_STATS_t conn_tbl_stats;
// Connection table stats snapshot, devices telemetry reads from it and
// festats thread writes into it when sees that the previous one has been
// processed and is ready to be reset.
static FE_TABLE_STATS_t conn_tbl_stats_snap;
// Flag indicating that the festats table statistics snapshot is being
// updated or read by the devices telemetry.
static int conn_tbl_stats_busy;
// Flag indicating that the festats table statistics snapshot is has been
// read by the devices telemetry.
static int conn_tbl_stats_read = TRUE;

// The list of monitored interfaces. Updated before each scan pass
// by fe_update_if_info()
static char lan_ifnames[TPCAP_IF_MAX][IFNAMSIZ];
// IP configuration of the monitored interfaces. Updated before
// each scan pass by fe_update_if_info()
static DEV_IP_CFG_t lan_ifcfg[TPCAP_IF_MAX];
// Number of interfaces in lan_ifcfg[] array
static int lan_ifcfg_count;

// Flag indicationg that we are starting up and have to collect
// the inital snapshort of the counters to initialize the offsets
static int first_pass = TRUE;


// Lock the connection. This lock only works for allowing devices telemetry
// thread collect the header, counters and clear the updated flag.
// wait - TRUE wait if currently busy, FALSE - return immediately
// Returns: TRUE - successfully locked, FALSE - busy (only if !wait)
static int fe_lock_conn(FE_CONN_t *conn, int wait)
{
    unsigned char flags = conn->flags;
    if(!wait) {
        if((flags & FE_CONN_BUSY) != 0) {
            return FALSE;
        }
        return __sync_bool_compare_and_swap(&(conn->flags), 
                                            flags, flags | FE_CONN_BUSY);
    }
    while((flags & FE_CONN_BUSY) != 0 ||
          !__sync_bool_compare_and_swap(&(conn->flags),
                                        flags, flags | FE_CONN_BUSY))
    {
        sleep(0);
        flags = conn->flags;
    }
    return TRUE;
}

// Lock the connection.
// wait - TRUE wait if currently busy, FALSE - return immediately
// Returns: TRUE - successfully locked, FALSE - busy (only if !wait)
static void fe_unlock_conn(FE_CONN_t *conn)
{
    __sync_synchronize();
    conn->flags &= ~FE_CONN_BUSY;
    return;
}

// Returns a copy of the connection table stats. 
// The "done" has to be FALSE when dev telemetry gets the pointer with
// data. The data is guaranteed to be unchanging at that point and till the
// function is called with "done" set to TRUE.
// Unlike similar .._tbl_stats() functions the reset is done automatically
// when the call has been made to capture the previous snapshot of the data.
// It will return null and will not reset if there was no new data snapshot
// done since the previous request.
DT_TABLE_STATS_t *fe_conn_tbl_stats(int done)
{
    if(!done) {
        // They want to work with the pointer, give them what we have and
        // set the busy flag so we do not touch it while thay are using it.
        if(!__sync_bool_compare_and_swap(&conn_tbl_stats_busy, FALSE, TRUE))
        {
            return NULL;
        }
        // Locked, give them the pointer
        return (DT_TABLE_STATS_t *)&conn_tbl_stats_snap;
    }

    // They are done with the pointer, set the read flag
    conn_tbl_stats_read = TRUE;

    // Reset the snapshot in case they try to use it again before we update
    memset(&conn_tbl_stats_snap, 0, sizeof(conn_tbl_stats_snap));

    // Now we can unlock
    __sync_synchronize();
    conn_tbl_stats_busy = FALSE;

    return NULL;
}

// Update table usage stats in conn_tbl_stats_snap if the previous
// snapshot is already consumed.
// For use only from the festats thread.
static void fe_conn_tbl_stats_snap(void)
{
    // Lock the table
    while(!__sync_bool_compare_and_swap(&conn_tbl_stats_busy, FALSE, TRUE))
    {
        sleep(0);
    }

    // Did they read what we posted last time? If yes post new data, clear the
    // read flag and reset the working copy.
    if(conn_tbl_stats_read) {
        memcpy(&conn_tbl_stats_snap, &conn_tbl_stats,
               sizeof(conn_tbl_stats_snap));
        memset(&conn_tbl_stats, 0, sizeof(conn_tbl_stats));
        conn_tbl_stats_read = FALSE;
    }

    // Now we can unlock
    __sync_synchronize();
    conn_tbl_stats_busy = FALSE;

    return;
}

// Add ARP entry into the table. It always succeedes.
// The old entries are just overridden.
// For use only from the festats thread.
static void fe_add_arp_entry(FE_ARP_t *ae)
{
    int ii;
    int idx = (ae->ipv4.b[2] << 8 | ae->ipv4.b[3]) % FESTATS_MAX_ARP;
    int sel_idx = idx;

    // Limit the number of slots we'll try, start at the index above
    for(ii = 0; ii < FESTATS_ARP_SEARCH_LIMITER; ii++)
    {
        // If empty
        if(arp_tbl[idx].ipv4.i == 0)
        {
            DBG_ARP("%s: new arp entry " IP_PRINTF_FMT_TPL " -> "
                    MAC_PRINTF_FMT_TPL " no_mac=%d\n",
                    __func__, IP_PRINTF_ARG_TPL(ae->ipv4.b),
                    MAC_PRINTF_ARG_TPL(ae->mac), ae->no_mac);
            sel_idx = idx;
            break;
        }
        // If the same address entry
        if(arp_tbl[idx].ipv4.i == ae->ipv4.i)
        {
            sel_idx = idx;
            break;
        }
        // Store the eldest entry index in sel_idx
        if(arp_tbl[idx].uptime < arp_tbl[sel_idx].uptime) {
            sel_idx = idx;
        }
        // Try the next index
        idx = (idx + 1) % FESTATS_MAX_ARP;
    }

    // Update the selected entry
    memcpy(&arp_tbl[sel_idx], ae, sizeof(arp_tbl[sel_idx]));

    return;
}

// Fills in lan_ifnames[][] array with LAN ifnames and the
// lan_ifcfg[] with the IP configuration of the monitored LAN interfaces.
static int fe_add_if(char *ifname, void *unused)
{
    DEV_IP_CFG_t *new_ipcfg = &lan_ifcfg[lan_ifcfg_count];

    // Capture the interface configuration
    memset(new_ipcfg, 0, sizeof(*new_ipcfg));
    if(util_get_ipcfg(ifname, new_ipcfg) != 0) {
        log("%s: failed to get IP config for %s\n", __func__, ifname);
        return -1;
    }

    // Capture the interface name
    strncpy(lan_ifnames[lan_ifcfg_count], ifname, IFNAMSIZ);
    lan_ifnames[lan_ifcfg_count][IFNAMSIZ - 1] = 0;

    ++lan_ifcfg_count;
    return 0;
}

// Update interface info that is used during the pass to determine
// which IP addresses and interface names are tracked on the LAN side
static void fe_update_if_info()
{
    // Prefill the iflist
    memset(lan_ifnames, 0, sizeof(lan_ifnames));

    // Capture monitored interface names and IP configuration
    lan_ifcfg_count = 0;
    util_enum_ifs(UTIL_IF_ENUM_RTR_LAN, fe_add_if, NULL);
    
    return;
}

// Find LAN interface by matching an IP address to its IP
// configuration.
// ifname - pointer to IFNAMSIZ buffer to receive the interface name found
//          (can be NULL)
// Returns: 0 - found, -1 - not found
static int fe_find_if_by_ipv4(IPV4_ADDR_t *ipv4, char *ifname)
{
    int ii;
    for(ii = 0; ii < lan_ifcfg_count; ++ii)
    {
        if((ipv4->i & lan_ifcfg[ii].ipv4mask.i) ==
           (lan_ifcfg[ii].ipv4.i & lan_ifcfg[ii].ipv4mask.i))
        {
            break;
        }
    }
    if(ii >= lan_ifcfg_count) {
        return -1;
    }
    if(ifname != NULL) {
        memcpy(ifname, lan_ifnames[ii], IFNAMSIZ);
    }
    return 0;
}

// Find IP config by matching LAN interface name to lan_ifnames[][].
// ipv4_cfg - pointer to DEV_IP_CFG_t buffer to receive the interface IP
//            configuration (can be NULL)
// Returns: 0 - found, -1 - not found
static int fe_find_ipv4cfg_by_ifname(char *ifname, DEV_IP_CFG_t *ipv4_cfg)
{
    int ii;
    for(ii = 0; ii < lan_ifcfg_count; ++ii)
    {
        if(strncmp(ifname, lan_ifnames[ii], IFNAMSIZ) == 0) {
            break;
        }
    }
    if(ii >= lan_ifcfg_count) {
        return -1;
    }
    if(ipv4_cfg != NULL) {
        memcpy(ipv4_cfg, &lan_ifcfg[ii], sizeof(*ipv4_cfg));
    }
    return 0;
}

// Update ARP tracker table. Example info from /proc/net/arp
// For use only from the festats thread.
// IP address       HW type     Flags       HW address            Mask     Device
// 10.4.0.1         0x1         0x2         26:f5:a2:a0:0b:22     *        vlan2
static void fe_update_arp_table()
{
    // Read info from the ARP cache
    char *arp_file = "/proc/net/arp";
#ifdef DEBUG
    if(*test_arp_file != 0) {
        arp_file = test_arp_file;
    }
#endif // DEBUG
    FILE *fp = fopen(arp_file, "r");
    if(!fp) {
        log("%s: error opening %s: %s\n", __func__, arp_file, strerror(errno));
        return;
    }
    char line[128];
    unsigned int ip[4];
    unsigned int mac[6];
    unsigned int hw, flags;
    char ifname[IFNAMSIZ];
    while(fgets(line, sizeof(line), fp) != NULL)
    {
        if(sscanf(line,
                  IP_PRINTF_FMT_TPL " %x %x " MAC_PRINTF_FMT_TPL " %*s %"
                  UTIL_STR(IFNAMSIZ) "s%*c",
                  &ip[0], &ip[1], &ip[2], &ip[3], &hw, &flags,
                  &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5],
                  ifname) != 13)
        {
            continue;
        }
        if(hw != 1 /* Ethernet */) {
            DBG_ARP("%s: skip entry w/ HW type %u\n", __func__, hw);
            continue;
        }
        if((flags & 2) == 0 /* Bit 1 - entry complete */) {
            DBG_ARP("%s: skip incomplete w/ flags %x\n", __func__, flags);
            continue;
        }
        ifname[sizeof(ifname) - 1] = 0;
        if(fe_find_ipv4cfg_by_ifname(ifname, NULL) != 0) {
            DBG_ARP("%s: skip entry for interface %s\n", __func__, ifname);
            continue;
        }
        FE_ARP_t ae = {.ipv4 = {.b = {ip[0], ip[1], ip[2], ip[3]}},
                       .uptime = util_time(1),
                       .mac = {mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]},
                       .no_mac = FALSE};
        // Add the new entry to the table
        fe_add_arp_entry(&ae);
    }
    fclose(fp);
    return;
}

// Query ARP table tracker for a match to specified IP address
// For use only from the festats thread.
static FE_ARP_t *fe_find_arp_entry(IPV4_ADDR_t *ipv4)
{
    int ii;
    int idx = (ipv4->b[2] << 8 | ipv4->b[3]) % FESTATS_MAX_ARP;

    for(ii = 0; ii < FESTATS_ARP_SEARCH_LIMITER; ii++)
    {
        // If empty or the same address entry
        if(arp_tbl[idx].ipv4.i == ipv4->i)
        {
            return &arp_tbl[idx];
        }
        // Try the next index
        idx = (idx + 1) % FESTATS_MAX_ARP;
    }

    return NULL;
}

// Walk through ARP tracker table trying to discover MACs for IPs that
// have connections, but no mapping to a MAC address is captured yet.
// This migh only happen at agebt restart or if a connection is set up
// after we scanned the ARP table, but before we scanned the connections.
void fe_run_arp_discovery(void)
{
    int ii;
    char ifname[IFNAMSIZ];
    for(ii = 0; ii < FESTATS_MAX_ARP; ++ii)
    {
        if(!arp_tbl[ii].no_mac) {
            continue;
        }
        DBG_ARP("%s: discovering MAC for " IP_PRINTF_FMT_TPL "\n",
                __func__, IP_PRINTF_ARG_TPL(arp_tbl[ii].ipv4.b));
        // Send ARP query
        if(fe_find_if_by_ipv4(&arp_tbl[ii].ipv4, ifname) != 0) {
            log("%s: no interface match for " IP_PRINTF_FMT_TPL "\n",
                __func__, IP_PRINTF_ARG_TPL(arp_tbl[ii].ipv4.b));
            continue;
        }
        util_send_arp_query(ifname, &arp_tbl[ii].ipv4);
    }
    return;
}

// Devices telemetry subsystem calls this function to collect connecton
// counters for the platforms that support IP forwarding acceleration.
// (for the ones that do not compile festats in it has an empty stub).
// Call this from the tpcap thread only.
// Note:
// If an entry in the festats connection table is locked, it will
// be skipped for the current pass and (hopefuly) will get added later.
void fe_report_conn(void)
{
    int ii;

    if(!conn_tbl) {
        // Not ready yet
        return;
    }

    for(ii = 0; ii < FESTATS_MAX_CONN; ++ii)
    {
        FE_CONN_t *conn = conn_tbl[ii];
        if(conn == NULL) {
            continue;
        }
        int flags = conn->flags;
        if((flags & (FE_CONN_UPDATED | FE_CONN_HAS_MAC)) != 
                    (FE_CONN_UPDATED | FE_CONN_HAS_MAC))
        {
            // Connection is either not updated or has no MAC address
            continue;
        }
        // The connection seems to have the info we need
        if(!fe_lock_conn(conn, FALSE)) {
            // Failed to lock without waiting, skip it till the next time
            continue;
        }
        // Note: normally flags should be checked again after locking, but
        //       since FE_CONN_UPDATED is never cleaned from festats and
        //       FE_CONN_HAS_MAC is never cleaned if FE_CONN_UPDATED is set
        //       we can skip the check for those.

        // If running festats test call its debug callback
#ifdef DEBUG
        if(get_test_num() == U_TEST_FESTATS) {
            int test_fe_add_conn(FE_CONN_t *conn);
            if(test_fe_add_conn(conn)) {
                conn->flags &= ~FE_CONN_UPDATED;
            }
        } else
#endif // DEBUG
        // Call devices telemetry to add the connection info
        if(dt_add_fe_conn(conn)) {
            // Successfully added info, clear the updated flag
            conn->flags &= ~FE_CONN_UPDATED;
        }

        fe_unlock_conn(conn);
    }

    return;
}

// Platform code can call this function to fix the connection header.
// It checks and swaps, if necessary, src/dst connection IP addresses
// & ports. It also updates the interface name where it was discovered.
// The intent is to allow platforms to prepare header structure by
// simply storing the connection IPs/ports in the header then call
// this function to finish setting all the remaining header fields.
// Parameters:
// hdr - pointer to the header to work on
// Returns:
// 0 - the header is ready, no reorder was needed
// positive - the header is ready, reordered src/dst
// negative - the header is not ready, ignore the connection
int fe_prep_conn_hdr(FE_CONN_HDR_t *hdr)
{
    int ret = 0;

    // Check if device IP is a local IP address
    memset(hdr->ifname, 0, sizeof(hdr->ifname));
    if(fe_find_if_by_ipv4(&hdr->dev_ipv4, hdr->ifname) != 0) {
        ret = 1;
        if(fe_find_if_by_ipv4(&hdr->peer_ipv4, hdr->ifname) != 0) {
            return -1;
        }
        // swap IP addresses
        IPV4_ADDR_t tmp_ipv4 = hdr->peer_ipv4;
        hdr->peer_ipv4 = hdr->dev_ipv4;
        hdr->dev_ipv4 = tmp_ipv4;
        // swap ports
        unsigned short tmp_port = hdr->peer_port;
        hdr->peer_port = hdr->dev_port;
        hdr->dev_port = tmp_port;
        // mark the entry as reverse
        hdr->rev = TRUE;
    } else {
        hdr->rev = FALSE;
    }

    return ret;
}

// Platform code calls this function to start an update (or addition) of a
// connection in the connection table. It finds or adds a connection, locks it
// and returns the pointer to the platform code. The platform code should update
// the counters (if changed) and call fe_upd_conn_end() to release the entry.
// The function should only be called from the festats thread.
// Parameters:
// hdr - header of the connection to find
// Returns:
// Pointer to conection entry or NULL if it's not found and cannot be added
FE_CONN_t *fe_upd_conn_start(FE_CONN_HDR_t *hdr)
{
    int ii, idx;
    FE_CONN_t *ret = NULL;

    // Total # of add requests
    ++(conn_tbl_stats.add_all);

    // Generate hash-based index
    idx = util_hash(hdr, sizeof(FE_CONN_HDR_t)) & (FESTATS_MAX_CONN - 1);

    // Limit the number of slots we'll try, start at the index above
    for(ii = 0; ii < FESTATS_CONN_SEARCH_LIMITER; ii++)
    {
        // If we hit an empty entry then the connection we are looking for
        // is not in the table (we should NEVER have gaps)
        if(!conn_tbl[idx]) {
            // New entry, allocate some memory for it
            ret = UTIL_MALLOC(sizeof(FE_CONN_t));
            if(!ret) { // Run out of memory
                ++(conn_tbl_stats.add_limit);
                return NULL;
            }
            ret->flags = 0;
            __sync_synchronize();
            conn_tbl[idx] = ret;
        }
        // Check if this is the same connection header
        else if(memcmp(&(conn_tbl[idx]->hdr), hdr, sizeof(FE_CONN_HDR_t)) == 0)
        {
            ret = conn_tbl[idx];
            ++(conn_tbl_stats.add_found);
            fe_lock_conn(ret, TRUE);
            break;
        }
        // Check if this is free entry and initialize it if so
        if(!conn_present[idx])
        {
            // Assign the connection in the table to the ret pointer.
            // If the connection is new, this is redundant but if the
            // connection is reused, it is necessary
            ret = conn_tbl[idx];
            // Lock the connection
            fe_lock_conn(ret, TRUE);
            // Copy the received header data into the entry's header
            memcpy(&(ret->hdr), hdr, sizeof(FE_CONN_HDR_t));
            // Zero the connection, but not the header and not the flags field
            memset((void *)ret + sizeof(FE_CONN_HDR_t), 0,
                   sizeof(FE_CONN_t)-sizeof(FE_CONN_HDR_t)-sizeof(ret->flags));
            // For the new connection entry only the busy flag should be set
            ret->flags = FE_CONN_BUSY;
            // Store the offset from the hash index position
            ret->off = ii;
            break;
        }

        // Try the next index
        idx = (idx + 1) & (FESTATS_MAX_CONN - 1);
    }

    if(ret) {
        // Mark the connection present
        conn_present[idx] = TRUE;
    } else {
        // Did not find a suitable entry for the connection
        ++(conn_tbl_stats.add_busy);
    }
    if(ii < 10) {
        ++(conn_tbl_stats.add_10);
    }

    return ret;
}

// Platform code calls this function to finish updating connection entry
// that has been returned by fe_upd_conn_start(). For the platforms that
// do not fill in MAC of the LAN device it tries to get it from ARP table.
// The function should only be called from the festats thread.
// Parameters:
// conn - pointer to connection being updated
// updated - TRUE if the counters changed, FALSE no changes,
//           but conection is still alive
// Returns:
// Pointer to conection entry or NULL if it's not found and cannot be added
void fe_upd_conn_end(FE_CONN_t *conn, int updated)
{
    // If MAC is not yet available try to get it from ARP
    if((conn->flags & FE_CONN_HAS_MAC) == 0) {
        FE_ARP_t *ae = fe_find_arp_entry(&conn->hdr.dev_ipv4);
        if(!ae) {
            // Maybe we have restarted and missed its ARP entry, add it to ARP
            // table with a flag indicating it has to be re-discovered.
            FE_ARP_t ae = {.ipv4 = conn->hdr.dev_ipv4,
                           .uptime = util_time(1),
                           .mac = {0, 0, 0, 0, 0, 0},
                           .no_mac = TRUE};
            fe_add_arp_entry(&ae);
        } else if(ae->no_mac) {
            // We already know about MAC being missing for this ARP entry.
        } else {
            memcpy(conn->mac, ae->mac, sizeof(conn->mac));
            conn->flags |= FE_CONN_HAS_MAC;
        }
    }
    // We have found or added the connection entry, set the updated flag
    if(updated) {
        if(!first_pass) {
            conn->flags |= FE_CONN_UPDATED;
        } else {
            // On the first pass we have to set the initial values in the
            // bytes_read fields (i.e. set the starting point for counting).
            conn->in.bytes_read = conn->in.bytes;
            conn->out.bytes_read = conn->out.bytes;
        }
    }
    // Unlock the entry
    fe_unlock_conn(conn);
    return;
}

// Helpers for defrag function finding the firs and last free entry in the
// connection table specified range of rows. The find_first searches forward.
// The find_last searches backward (starting at "from - 1").
// from - start position
// count - how many rows to search
// Returns: entry index or -1
static int defrag_find_first_free(int from, int count)
{
    int ii;
    int ret = -1;

    for(ii = 0; ii < count; ++ii)
    {
        int idx = (from + ii) & (FESTATS_MAX_CONN - 1);
        if(!conn_present[idx]) {
            ret = idx;
            break;
        }
    }

    return ret;
}
static int defrag_find_last_free(int from, int count)
{
    int ii;
    int ret = -1;

    for(ii = 0; ii < count; ++ii)
    {
        int idx = (from - (ii + 1)) & (FESTATS_MAX_CONN - 1);
        if(!conn_present[idx]) {
            ret = idx;
            break;
        }
    }

    return ret;
}

// Defragment the connection table and for no longer present connections
// clear header. Each table entry that is placed into some row further than
// pointed by its hash must have all the rows above it and up to its
// best position filled. The function walks through the table and
// for all entries that have a gap in the rows leading to their
// current position moves them to avoid the gap.
static void defrag_and_clean_table(void)
{
    int ii, ii_limiter = FESTATS_MAX_CONN;
    int last_free = defrag_find_last_free(0, FESTATS_CONN_SEARCH_LIMITER);

    // Before starting the defrag loop make sure we do not have unread
    // entries marked not present anywhere in the table up to
    // FESTATS_CONN_SEARCH_LIMITER back from the position we will start at.
    for(ii = 0; ii < FESTATS_CONN_SEARCH_LIMITER; ++ii) {
        int idx = (~ii) & (FESTATS_MAX_CONN - 1);
        if(conn_tbl[idx] && (conn_tbl[idx]->flags & FE_CONN_UPDATED) != 0) {
            conn_present[idx] = TRUE;
        }
    }

    // Main defrag loop
    for(ii = 0; ii < ii_limiter; ++ii)
    {
        int idx = ii & (FESTATS_MAX_CONN - 1);
        FE_CONN_t *conn = conn_tbl[idx];

        if(conn == NULL) {
            last_free = idx;
            continue;
        }

        // Keep the connections that are gone, but not yet reported
        if((conn->flags & FE_CONN_UPDATED) != 0) {
            conn_present[idx] = TRUE;
        }
        else if(!conn_present[idx]) {
            last_free = idx;
            // We can clean without locking since the FE_CONN_UPDATED flag could
            // be set only from festats thread.
            // Clean just the dev_ip, it's enough to make assure "no match".
            conn->hdr.dev_ipv4.i = 0;
            continue;
        }

        // Calculate the best position for the entry
        int best_pos = (idx - conn->off) & (FESTATS_MAX_CONN - 1);
        if(last_free < 0) {
            // no free positions, the entry can stay
            continue;
        }

#if ((FESTATS_MAX_CONN - 1) ^ (FESTATS_MAX_CONN)) + 1 != FESTATS_MAX_CONN << 1
#  error FESTATS_MAX_CONN must be power of 2
#endif // quick check (unreliable) for the number to be ^2

        // Check if the last free position is within the range of
        // the records from the entry best to its current position.
        int dst_to_last_free = (idx - last_free) & (FESTATS_MAX_CONN - 1);
        int dst_to_best = (idx - best_pos) & (FESTATS_MAX_CONN - 1);
        if(dst_to_best < dst_to_last_free) {
            // no free positions up to the best row, the entry can stay
            continue;
        }

        // We have to move the entry, find the first available position
        // from its best.
        int new_idx = defrag_find_first_free(best_pos,
                                             FESTATS_CONN_SEARCH_LIMITER);
        if(new_idx < 0) {
            DBG_DFG("%s: internal error, cur:%d b:%d l:%d bd:%d ld:%d\n",
                    __func__, idx, best_pos, last_free,
                    dst_to_best, dst_to_last_free);
            continue;
        }

        // Move the entry (swap)
        conn->off = (new_idx - best_pos) & (FESTATS_MAX_CONN - 1);
        conn_tbl[idx] = conn_tbl[new_idx];
        conn_tbl[new_idx] = conn;

        // The current entry is now free
        conn_present[new_idx] = TRUE;
        conn_present[idx] = FALSE;
        last_free = idx;

        //DBG_DFG("%s: swapped: %d <-> %d\n", __func__, idx, new_idx);
        
        // Since we just freed an entry, we might have created a gap
        // that has not been there before, so the loop migh have to
        // check more entries beyond the inital limit. We only start 
        // changing the ii_limiter when the current position reaches 
        // FESTATS_MAX_CONN - FESTATS_CONN_SEARCH_LIMITER
        if(ii > FESTATS_MAX_CONN - FESTATS_CONN_SEARCH_LIMITER)
        {
            ii_limiter = ii + FESTATS_CONN_SEARCH_LIMITER;
            DBG_DFG("%s: new ii_limiter: %d\n", __func__, ii_limiter);
        }

        // Do we need to limit runaway defrag? It can be done by starting to
        // keep rows taken by phony entries after swap if seeing ii_limiter
        // growing to high. I'll not do that yet.
    }

    return;
}

// This function puts together all actions for a single festats pass
static void fe_stats_pass(void)
{
    // Update interface info
    fe_update_if_info();

    // Update ARP tracker table
    fe_update_arp_table();

    // Mark all connection entries as not longer present
    memset(conn_present, 0, FESTATS_MAX_CONN * sizeof(*conn_present));

    // Update counters and mark present connections.
    // Once counters are updated, we'll have the entire rest of the
    // time slice to carry out further operations, ideally being able
    // to maintain constant intervals.
    fe_platform_update_stats();

    // Each table entry that has to be placed into some row further than
    // pointed by its hash must have all the rows above it and up to its
    // best position filled. After the update the rows of the connections
    // that are no longer present are considered free and the table must
    // be defragmented to avoid any such gaps.
    defrag_and_clean_table();

    // Try to discover missing MAC addresses
    fe_run_arp_discovery();

    // Update table usage stats snapshot.
    fe_conn_tbl_stats_snap();

    return;
}

// Main IP forwarding stats collector loop
static void festats(THRD_PARAM_t *p)
{
    log("%s: started\n", __func__);

    log("%s: waiting for activate to complete\n", __func__);
    wait_for_activate();
    log("%s: done waiting for activate\n", __func__);
    
    for(;;) {
        unsigned long t_end = util_time(1000) + FESTATS_INTERVAL_MSEC;

        // Do a pass collecting fast forwarding engine stats
        fe_stats_pass();

        // That completes the first pass
        first_pass = FALSE;

        // Wait for the current time slice's remaining duration
        unsigned long t_sleep = t_end - util_time(1000);
        if(t_sleep <= FESTATS_INTERVAL_MSEC) {
            util_msleep(t_sleep);
        } else {
            log("%s: iteration took %ul more that %d ms\n",
                __func__, (~t_sleep) + 1, FESTATS_INTERVAL_MSEC);
        }
    }

    return;
}

// Free tables memory
static void fe_free_tables(void)
{
    if(arp_tbl) {
        UTIL_FREE(arp_tbl);
        arp_tbl = NULL;
    }
    if(conn_present) {
        UTIL_FREE(conn_present);
        conn_present = NULL;
    }
    if(conn_tbl)
    {
        int ii;
        for(ii = 0; ii < FESTATS_MAX_CONN; ++ii)
        {
            FE_CONN_t *conn = conn_tbl[ii];
            if(conn == NULL) {
                continue;
            }
            conn_tbl[ii] = NULL;
            UTIL_FREE(conn);
        }
        UTIL_FREE(conn_tbl);
        conn_tbl = NULL;
    }

    return;
}

// Allocate memory and initialize tables
static int fe_allocate_tables(void)
{
    conn_tbl = UTIL_CALLOC(FESTATS_MAX_CONN, sizeof(*conn_tbl));
    conn_present = UTIL_CALLOC(FESTATS_MAX_CONN, sizeof(*conn_present));
    if(conn_tbl == NULL || conn_present == NULL) {
        fe_free_tables();
        log("%s: unable to allocate memory for connections\n", __func__);
        return -1;
    }
    arp_tbl = UTIL_CALLOC(FESTATS_MAX_ARP, sizeof(*arp_tbl));
    if(arp_tbl == NULL) {
        fe_free_tables();
        log("%s: unable to allocate memory for ARP tracker\n", __func__);
        return -2;
    }

    return 0;
}

// Initialize the IP forwarding stats collector subsystem
int fe_start_subsystem(void)
{
    if(fe_allocate_tables() != 0) {
        return -1;
    }
    if(util_start_thrd("festats", festats, NULL, NULL) != 0) {
        fe_free_tables();
        return -2;
    }

    return 0;
}


#ifdef DEBUG
// These variables are used by festats test to set custom
// paths to the files for ARP and platform connection trackers.
char test_arp_file[80];
char test_conn_file[80];

// A few protocol names
static char *proto_name(unsigned char proto)
{
    char *n = "unknown";
    switch(proto) {
        case 1:  n = "ICMP";     break;
        case 2:  n = "IGMP";     break;
        case 6:  n = "TCP";      break;
        case 17: n = "UDP";      break;
        case 47: n = "GRE";      break;
        case 51: n = "IPSecAH";  break;
        case 50: n = "IPSecESP"; break;
        case 8:  n = "EGP";      break;
        case 3:  n = "GGP";      break;
        case 20: n = "HMP";      break;
        case 88: n = "IGMP";     break;
        case 66: n = "RVD";      break;
        case 89: n = "OSPF";     break;
        case 12: n = "PUP";      break;
        case 27: n = "RDP";      break;
        case 46: n = "RSVP";     break;
    }
    return n;
}

// Test festats defrag
int test_fe_defrag(void)
{
    char str[12];

    if(fe_allocate_tables() != 0) {
        printf("Unable to allocate memory\n");
        return -1;
    }

    for(;;)
    {
        unsigned int fill_prc = 0;
        unsigned int offset_prc = 0;

        printf("Enter percentage <fill>/<max offset>:\n");
        scanf("%12s", str);
        str[sizeof(str) - 1] = 0;
        if(sscanf(str, "%u/%u", &fill_prc, &offset_prc) != 2 ||
           fill_prc > 100 || offset_prc > 100)
        {
            continue;
        }

        unsigned int fill = (FESTATS_MAX_CONN * fill_prc) / 100;
        unsigned int max_off = (FESTATS_CONN_SEARCH_LIMITER *
                                offset_prc) / 100;

        // Fill in the tables with fake entries randomly picking offset
        // off the best position for each entry.
        memset(conn_present, 0, FESTATS_MAX_CONN * sizeof(*conn_present));
        int ii;
        for(ii = 0; ii < fill; ++ii)
        {
            int idx = rand() & (FESTATS_MAX_CONN - 1);
            int off = max_off ? (rand() % max_off) : 0;

            while(conn_tbl[idx]) {
                idx = (idx + 1) & (FESTATS_MAX_CONN - 1);
            }
            FE_CONN_t *conn = UTIL_MALLOC(sizeof(FE_CONN_t));
            memset(conn, 0, sizeof(*conn));
            sprintf((char *)conn->mac, "%6d", idx);
            conn->off = off;
            conn_tbl[idx] = conn;
            conn_present[idx] = TRUE;
        }
        printf("Added %d entries...\n", fill);

        // Run the defrag
        unsigned long tt_start = util_time(1000);
        defrag_and_clean_table();
        unsigned long tt_end = util_time(1000);
        
        // Check the table state
        printf("Defrag done in %lu msec, checking the table...\n",
               tt_end - tt_start);
        for(ii = 0; ii < FESTATS_MAX_CONN; ++ii)
        {
            int idx = ii;
            FE_CONN_t *conn = conn_tbl[idx];
            while((conn != NULL) ^ (conn_present[idx] != FALSE)) {
                printf("Error at %d, ptr:%p(%s) taken:%d\n",
                       idx, conn, (conn ? (char *)conn->mac : ""),
                       conn_present[idx]);
            }
            if(!conn || conn->off == 0) {
                continue;
            }
            int free_idx = defrag_find_last_free(idx,
                                                 FESTATS_CONN_SEARCH_LIMITER);
            int free_idx_dst = (idx - free_idx) & (FESTATS_MAX_CONN - 1);
            if(free_idx > 0 && free_idx_dst <= conn->off) {
                printf("Error at %d, ptr:%p(%s) off:%d free_dst:%d free:%d\n",
                       idx, conn, (conn ? (char *)conn->mac : ""),
                       conn->off, free_idx_dst, free_idx);
            }
        }
        printf("Done checking the table.\n");

        // Cleanup after the test run
        printf("Cleanup...\n");
        for(ii = 0; ii < FESTATS_MAX_CONN; ++ii) {
            int idx = ii;
            FE_CONN_t *conn = conn_tbl[idx];
            if(conn == NULL) {
                continue;
            }
            conn_tbl[idx] = NULL;
            UTIL_FREE(conn);
            conn_present[idx] = FALSE;
        }
        printf("Done.\n");
    }
    
    return 0;
}

// Test festats ARP tracker
int test_fe_arp(void)
{
    char str[16];
    unsigned int ip[4];

    if(fe_allocate_tables() != 0) {
        printf("Unable to allocate memory\n");
        return -1;
    }

    for(;;)
    {
        printf("1 - run ARP tracker pass, 2 - discover, <IP> - run query:\n");
        scanf("%16s", str);
        str[sizeof(str) - 1] = 0;

        // Update interface info
        fe_update_if_info();

        if(sscanf(str, IP_PRINTF_FMT_TPL, &ip[0], &ip[1], &ip[2], &ip[3]) == 4)
        {
            IPV4_ADDR_t ipv4 = {.b = {ip[0], ip[1], ip[2], ip[3]}};
            printf("Looking up: " IP_PRINTF_FMT_TPL "\n",
                   IP_PRINTF_ARG_TPL(ipv4.b));
            FE_ARP_t *ae = fe_find_arp_entry(&ipv4);
            if(ae != NULL) {
                printf("ARP entry " IP_PRINTF_FMT_TPL " -> "
                       MAC_PRINTF_FMT_TPL " no_mac=%d\n",
                       IP_PRINTF_ARG_TPL(ae->ipv4.b),
                       MAC_PRINTF_ARG_TPL(ae->mac), ae->no_mac);
                for(;;) {
                    printf("1 - set no_mac, 2 - skip:\n");
                    scanf("%2s", str);
                    if(*str == '1') {
                        ae->no_mac = TRUE;
                        memset(ae->mac, 0, sizeof(ae->mac));
                        break;
                    }
                    else if(*str == '2') {
                        break;
                    }
                }
            } else {
                printf("No ARP info entry\n");
            }
        }
        else if(*str == '1')
        {
            printf("Scanning ARP table...\n");
            fe_update_arp_table();
        }
        else if(*str == '2')
        {
            printf("Run MACs discovery...\n");
            fe_run_arp_discovery();
        }
    }
    
    return 0;
}

// Print out details for a single connection
static void print_fe_conn_info(FE_CONN_t *conn)
{
    printf("%3u/%.8s " IP_PRINTF_FMT_TPL ":%u %s " IP_PRINTF_FMT_TPL ":%u\n",
           conn->hdr.proto, proto_name(conn->hdr.proto),
           IP_PRINTF_ARG_TPL(conn->hdr.dev_ipv4.b), conn->hdr.dev_port,
           (conn->hdr.rev ? "<-" : "->"),
           IP_PRINTF_ARG_TPL(conn->hdr.peer_ipv4.b), conn->hdr.peer_port);
    printf("  uid: %u off:%d flags:0x%x\n", conn->uid, conn->off, conn->flags);
    printf("  bytes in/out: %llu/%llu\n", conn->in.bytes, conn->out.bytes);
    printf("  read in/out: %llu/%llu\n", conn->in.bytes_read,
                                         conn->out.bytes_read);

    return;
}

// Call back from festats reporting updated connection
int test_fe_add_conn(FE_CONN_t *conn)
{
    char str[4];
    print_fe_conn_info(conn);
    for(;;) {
        printf("1 - process, 2 - skip:\n");
        scanf("%2s", str);
        if(*str == '1') {
            break;
        }
        if(*str == '2') {
            return FALSE;
        }
    }
    printf("  reported bytes in/out: %llu/%llu\n",
           conn->in.bytes - conn->in.bytes_read,
           conn->out.bytes - conn->out.bytes_read);
    conn->in.bytes_read = conn->in.bytes;
    conn->out.bytes_read = conn->out.bytes;
    return TRUE;
}

// Test connection reporting
static void report_test(void)
{
    printf("Requesting connection report\n");
    printf("------------------------------------------------\n");
    fe_report_conn();
    printf("------------------------------------------------\n");
    printf("\n");
}

// Run a scan pass and print out the festats connections info
static void scan_pass_test(void)
{
    int ii, count = 0;

    printf("Executing the pass (first:%d)\n", first_pass);
    printf("------------------------------------------------\n");
    // Do a pass collecting fast forwarding engine stats
    fe_stats_pass();
    first_pass = FALSE;
    printf("------------------------------------------------\n");

    printf("Done executing the pass, connections table dump:\n");
    printf("------------------------------------------------\n");
    for(ii = 0; ii < FESTATS_MAX_CONN; ii++) {
        FE_CONN_t *conn = conn_tbl[ii];
        if(!conn) {
            continue;
        }
        if(conn_present[ii]) {
            count++;
            print_fe_conn_info(conn);
        }
    }
    printf("------------------------------------------------\n");
    printf("Connections: %d\n\n", count);
    
    return;
}

// Report, then print festats table stats and then reset them
static void scan_stats_test(void)
{
    FE_TABLE_STATS_t *conn_stats = fe_conn_tbl_stats(FALSE);
    
    printf("Connections table stats (reset requested after printout):\n");
    printf("  add_all   = %lu\n", conn_stats->add_all);
    printf("  add_limit = %lu\n", conn_stats->add_limit);
    printf("  add_busy  = %lu\n", conn_stats->add_busy);
    printf("  add_10    = %lu\n", conn_stats->add_10);
    printf("  add_found = %lu\n", conn_stats->add_found);
    printf("\n");

    // Release the lock on the data
    fe_conn_tbl_stats(TRUE);

    return;
}

// Set ARP and connection tracking files for various tests
void test_festats_set_src_files(void)
{
    char str[4];
    char *test_arp = "/var/fe_arp";
    char *test_conn = "/var/fe_conn";

    for(;;) {
        printf("ARP tracker file (1 - default, 2 - %s):\n", test_arp);
        scanf("%2s", str);
        if(*str == '1') {
            *test_arp_file = 0;
            break;
        } else if(*str == '2') {
            strncpy(test_arp_file, test_arp, sizeof(test_arp_file));
            break;
        }
    }
    for(;;) {
        printf("Connection tracker file (1 - default, 2 - %s):\n", test_conn);
        scanf("%2s", str);
        if(*str == '1') {
            *test_conn_file = 0;
            break;
        } else if(*str == '2') {
            strncpy(test_conn_file, test_conn, sizeof(test_conn_file));
            break;
        }
    }

    return;
}

// Start the festats subsystem and print diagnostic information to
// stdout in a loop, forever.
int test_festats()
{
    printf("sizeof FE_COUNTER_t  = %d\n", sizeof(FE_COUNTER_t));
    printf("sizeof FE_CONN_HDR_t = %d\n", sizeof(FE_CONN_HDR_t));
    printf("sizeof FE_CONN_t     = %d\n", sizeof(FE_CONN_t));
    printf("sizeof FE_ARP_t      = %d\n", sizeof(FE_ARP_t));

    if(fe_allocate_tables() != 0) {
        printf("Unable to allocate memory\n");
        return -1;
    }

    test_festats_set_src_files();

    for(;;)
    {
        char str[4];

        printf("1 - scan connections, 2 - report, 3 - stats:\n");
        scanf("%2s", str);
        if(*str == '1') {
            scan_pass_test();
        }
        else if(*str == '2') {
            report_test();
        }
        else if(*str == '3') {
            scan_stats_test();
        }
    }

    return 0;
}
#endif // DEBUG
