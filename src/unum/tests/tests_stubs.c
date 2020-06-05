// (c) 2017-2019 minim.co
// unum device specifc test stubs
// using weak reference so platforms can implement their own

#include "unum.h"

// Compile only in debug version
#ifdef DEBUG

// Test load config
int __attribute__((weak)) test_loadCfg(void)
{
    printf("Test %d is not implemented for the platform\n",
           get_test_num());
    return 0;
}

// Test save config
int __attribute__((weak)) test_saveCfg(void)
{
    printf("Test %d is not implemented for the platform\n",
           get_test_num());
    return 0;
}

// Test wireless
void __attribute__((weak)) test_wireless(void)
{
    printf("Test %d is not implemented for the platform\n",
           get_test_num());
}

// Test festats
int __attribute__((weak)) test_festats(void)
{
    printf("Test %d is not implemented for the platform\n",
           get_test_num());
    return 0;
}

// Test festats defrag
int __attribute__((weak)) test_fe_defrag(void)
{
    printf("Test %d is not implemented for the platform\n",
           get_test_num());
    return 0;
}

// Test festats ARP tracker
int __attribute__((weak)) test_fe_arp(void)
{
    printf("Test %d is not implemented for the platform\n",
           get_test_num());
    return 0;
}

// Test TCP port scanner
void __attribute__((weak)) test_port_scan(void)
{
    printf("Test %d is not implemented for the platform\n",
           get_test_num());
    return;
}
#endif // DEBUG
