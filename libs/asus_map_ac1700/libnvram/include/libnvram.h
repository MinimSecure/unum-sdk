#ifndef _NVRAM_H
#define _NVRAM_H

// Some prototypes are taken from Asus SDK to use these
// functions in unum-v2
const char *nvram_get(char *name);
int nvram_set(const char *name, const char *value);
int nvram_commit(void);
int nvram_getall(char *nvram_buf, int count);
extern int nvram_get_int(const char *key);
extern int nvram_set_int(const char *key, int value);
extern int nvram_unset(const char *name);

#define MAX_NVRAM_SPACE 0x8000

#endif // _NVRAM_H
