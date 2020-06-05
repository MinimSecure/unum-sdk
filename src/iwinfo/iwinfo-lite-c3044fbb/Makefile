IWINFO_BACKENDS    = $(BACKENDS)
IWINFO_CFLAGS      = $(CFLAGS) -std=gnu99 -fstrict-aliasing -Iinclude
IWINFO_LDFLAGS     = $(LDFLAGS)

IWINFO_LIB         = libiwinfo.so
IWINFO_LIB_LDFLAGS = -shared
IWINFO_LIB_OBJ     = iwinfo_utils.o iwinfo_wext.o iwinfo_wext_scan.o iwinfo_lib.o

ifneq ($(LUA),)
	IWINFO_LUA         = iwinfo.so
	IWINFO_LUA_LDFLAGS = -shared -L. -liwinfo -l$(LUA)
	IWINFO_LUA_OBJ     = iwinfo_lua.o
endif

IWINFO_CLI         = iwinfo
IWINFO_CLI_LDFLAGS = -L. -liwinfo
IWINFO_CLI_OBJ     = iwinfo_cli.o


ifneq ($(filter wl,$(IWINFO_BACKENDS)),)
	IWINFO_CFLAGS  += -DUSE_WL
	IWINFO_LIB_OBJ += iwinfo_wl.o
endif

ifneq ($(filter madwifi,$(IWINFO_BACKENDS)),)
	IWINFO_CFLAGS  += -DUSE_MADWIFI
	IWINFO_LIB_OBJ += iwinfo_madwifi.o
endif

ifneq ($(filter nl80211,$(IWINFO_BACKENDS)),)
	IWINFO_CFLAGS  += -DUSE_NL80211
	IWINFO_LIB_OBJ += iwinfo_nl80211.o
endif

%.o: %.c
	$(CC) $(IWINFO_CFLAGS) -fPIC -c -o $@ $<

compile: clean $(IWINFO_LIB_OBJ) $(IWINFO_CLI_OBJ) $(IWINFO_LUA_OBJ)
	$(CC) -o $(IWINFO_LIB) $(IWINFO_LIB_OBJ) $(IWINFO_LDFLAGS) $(IWINFO_LIB_LDFLAGS)
	$(CC) -o $(IWINFO_CLI) $(IWINFO_CLI_OBJ) $(IWINFO_LDFLAGS) $(IWINFO_CLI_LDFLAGS)
ifneq ($(WINFO_LUA),)
	$(CC) -o $(IWINFO_LUA) $(IWINFO_LUA_OBJ) $(IWINFO_LDFLAGS) $(IWINFO_LUA_LDFLAGS)
endif

clean:
	rm -f *.o $(IWINFO_LIB) $(IWINFO_LUA) $(IWINFO_CLI)
