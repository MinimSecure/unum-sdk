// (c) 2019 minim.co
// unum device specifc tests

#include "unum.h"

// Compile only in debug version
#ifdef DEBUG

// Test config file name
static char *cfg_fname = "/tmp/nvram.txt";

// Test load config()
int test_loadCfg(void)
{
    int ret;
    char *buf;
    int len;
    FILE *f;

    printf("Calling platform_cfg_init()...");
    ret = platform_cfg_init();
    printf(" %s(%d)\n", ret ? "error" : "success", ret);
    if(ret != 0) {
        return 1;
    }

    printf("Calling platform_cfg_get()...\n");
    buf = platform_cfg_get(&len);

    printf("Config len: %d, data ptr: %p\n", len, buf);
    if(buf && len > 0) {
        printf("Content:\n%.256s...\n", buf);
    }

    printf("Saving cfg to: <%s>\n", cfg_fname);
    f = fopen(cfg_fname, "w");
    if(!f) {
        printf("Failed to create %s: %s\n", cfg_fname, strerror(errno));
    } else {
        ret = fwrite(buf, 1, len, f);
        fclose(f);
        printf("Saved %d config bytes in %s\n", ret, cfg_fname);
    }

    printf("Releasing the config buffer...\n");
    platform_cfg_free(buf);

    return 0;
}

// Test save config
int test_saveCfg(void)
{
    struct stat st;
    int ret;
    int buf_len, len;
    char *buf = NULL;
    FILE *f;

    printf("Calling platform_cfg_init()...");
    ret = platform_cfg_init();
    printf(" %s(%d)\n", ret ? "error" : "success", ret);
    if(ret != 0) {
        return 1;
    }

    printf("Loading cfg from %s\n", cfg_fname);
    f = fopen(cfg_fname, "r");
    if(!f) {
        printf("Failed to open %s: %s\n", cfg_fname, strerror(errno));
        return 0;
    }
    if(fstat(fileno(f), &st) != 0) {
        printf("Error doing stat on %s: %s\n", cfg_fname, strerror(errno));
        return 0;
    }
    buf_len = st.st_size + 1;
    buf = malloc(buf_len);
    ret = fread(buf, 1, buf_len, f);
    fclose(f);
    printf("Loaded %d config bytes\n", ret);
    buf[buf_len - 1] = 0;
    len = strlen(buf);
    printf("Config len (w/o terminating zero): %d\n", len);
    printf("Content:\n%.256s...\n", buf);

    printf("Applying config...\n");
    ret = platform_apply_cloud_cfg(buf, buf_len);

    free(buf);
    return 0;
}

#endif // DEBUG
