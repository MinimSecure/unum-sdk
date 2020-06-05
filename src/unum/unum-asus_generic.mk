# (c) 2019-2020 minim.co
# Platform makefile for unum agent executable build

# Add linking flags for libs that we built before the agent
# (those migh differ for the platforms, so include conditionally
# in case we need to share these additions a lot).

CPPFLAGS += -DUSE_OPEN_SSL

ifneq ($(CURL),)
  CPPFLAGS += -I$(TARGET_OBJ)/curl/$(CURL)/include
  LDFLAGS += -L$(TARGET_OBJ)/curl/$(CURL)/lib/.libs -l:libcurl.so
  #LDFLAGS += -L$(TARGET_OBJ)/curl/$(CURL)/lib/.libs -l:libcurl.a
endif
ifneq ($(JANSSON),)
  CPPFLAGS += -I$(TARGET_OBJ)/jansson/$(JANSSON)/src
  LDFLAGS += -L$(TARGET_OBJ)/jansson/$(JANSSON)/src/.libs -ljansson -lm
endif
ifneq ($(IWINFO),)
  CPPFLAGS += -I$(TARGET_OBJ)/iwinfo/$(IWINFO)/include/
  LDFLAGS += -L$(TARGET_OBJ)/iwinfo/$(IWINFO) -liwinfo
endif

# Add hardware kind define
CPPFLAGS += -DDEVICE_PRODUCT_NAME=\"$(MODEL)\"
CPPFLAGS += -DWT_IWINFO_NO_MCS -DWT_IWINFO_NO_TXRETRY
