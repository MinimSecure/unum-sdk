// (c) 2019 minim.co
// Global stubs, mainly for top level init functions

#include "unum.h"

// Stubs for the platforms that do not compile in fingerprinting.
int __attribute__((weak)) fp_init(int level)
{
    return -1;
}
JSON_KEYVAL_TPL_t __attribute__((weak)) *fp_mk_json_tpl_f(char *key)
{
    return NULL;
}
void __attribute__((weak)) fp_reset_tables(void)
{
    return;
}
