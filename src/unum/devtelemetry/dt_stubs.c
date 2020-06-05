// (c) 2018 minim.co
// device telemetry information collector stubs

#include "unum.h"


// Stub for the platforms that do not compile in festats subsystem.
// Always return NULL which should prevent inclusion of the data.
FE_TABLE_STATS_t * __attribute__((weak)) fe_conn_tbl_stats(int done)
{
    // Nothing to do here
    return NULL;
}

// Stub for the platforms that do not compile in festats subsystem
void __attribute__((weak)) fe_report_conn(void)
{
    // Nothing to do here
    return;
}
