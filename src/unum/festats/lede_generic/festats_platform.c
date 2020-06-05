// (c) 2018 minim.co
// platform specific code for fe stats gathering

#include "unum.h"

/* Temporary, log to console from here */
//#undef LOG_DST
//#undef LOG_DBG_DST
//#define LOG_DST LOG_DST_CONSOLE
//#define LOG_DBG_DST LOG_DST_CONSOLE


// TODO: this code has to be cleaned up


// Proc FS file to get SFE stats from
#define FESTATS_SFE_PROCFILE "/proc/minimconnections"


// Function called by common code to start the IP forwarding
// engine stats collection pass.
void fe_platform_update_stats()
{
    static char mac_buf[MAC_ADDRSTRLEN];
    static unsigned int m[6];
    FILE *fp_stats;
    unsigned char mac[6];
    unsigned long txb, rxb, txp, rxp;
    int proto;
    int sport;
    int dport;
    int srcip;
    int dstip;
    FE_CONN_t *conn;
    int ii;

    char *file = FESTATS_SFE_PROCFILE;
#ifdef DEBUG
    if(*test_conn_file != 0) {
        file = test_conn_file;
    }
#endif // DEBUG
    fp_stats = fopen(file, "r");
    if(!fp_stats) {
        log("%s: error opening %s: %s\n", __func__, file, strerror(errno));
        return;
    }

    // TODO: use %17s or better use fgets() and then scan MAC to make sure the
    //       string is not corrupt and, after that strchr/strtok and
    //       strtol()/strtoul() to read remaining integers from the string.
    //       BTW, since we are in control of all this it doesn't even have to be
    //       text, SFE can just prep the data exactly as we need it. This will be
    //       much faster than scanning text.
    while(fscanf(fp_stats, "%s %d %d %d %d %d %lu %lu %lu %lu",
                 mac_buf, &proto, &dstip, &dport, &srcip,
                 &sport, &txb, &txp, &rxb, &rxp) == 10)
    {
        if(sscanf((const char *) mac_buf, MAC_PRINTF_FMT_TPL,
                  &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6)
        {
            log("%s: failed to parse device mac address: %s\n", __func__, mac_buf);
            continue;
        }
        for(ii = 0; ii < 6; ++ii) {
            mac[ii] = (unsigned char)m[ii];
        }

        FE_CONN_HDR_t hdr;
        hdr.proto = proto;
        hdr.dev_ipv4.i = srcip;
        hdr.peer_ipv4.i = dstip;
        hdr.dev_port = sport;
        hdr.peer_port = dport;

        if(fe_prep_conn_hdr(&hdr) < 0) {
            log_dbg("%s: skipping p:%d " IP_PRINTF_FMT_TPL ":%hu->"
                    IP_PRINTF_FMT_TPL ":%hu\n", __func__,
                    hdr.proto, IP_PRINTF_ARG_TPL(hdr.dev_ipv4.b), hdr.dev_port,
                    IP_PRINTF_ARG_TPL(hdr.peer_ipv4.b), hdr.peer_port);
            continue;
        }

        conn = fe_upd_conn_start(&hdr);
        if(!conn) {
            log("%s: cannot add p:%d " IP_PRINTF_FMT_TPL ":%hu->"
                IP_PRINTF_FMT_TPL ":%hu\n", __func__,
                hdr.proto, IP_PRINTF_ARG_TPL(hdr.dev_ipv4.b), hdr.dev_port,
                IP_PRINTF_ARG_TPL(hdr.peer_ipv4.b), hdr.peer_port);
            continue;
        }

        // We have the device MAC address!!!
        if(memcpy(conn->mac, mac, sizeof(conn->mac)) != 0) {
            conn->flags |= FE_CONN_HAS_MAC;
        }

        // SFE counters are reset on each read, so we just add what it reports
        // to the values captured so far.
        if(conn->hdr.rev) { // Reverse, swap Tx/Rx
            conn->in.bytes += txb;
            conn->out.bytes += rxb;
        } else {        // Normal src==LAN, dst==WAN connection
            conn->in.bytes += rxb;
            conn->out.bytes += txb;
        }

        fe_upd_conn_end(conn, (txb != 0 || rxb != 0));

        // Discard our pointer to the entry
        conn = NULL;
    }
    fclose(fp_stats);

    log("%s: done updating sfe counters\n", __func__);
    
    return;
}
