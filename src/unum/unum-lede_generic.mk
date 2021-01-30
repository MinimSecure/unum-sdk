# (c) 2017-2019 minim.co
# Platform makefile for unum agent executable build

# Add linking flags for libs, they should be available 
# since our package depends on those libs

# Generic libs
LDFLAGS += -lm -lrt

# Mbedtls
LDFLAGS += -lmbedtls -lmbedx509 -lmbedcrypto

# Curl
LDFLAGS += -lcurl

# Jansson
LDFLAGS += -ljansson

# UCI library
LDFLAGS += -luci

# Wireless
LDFLAGS += -liwinfo

# Zlib
LDFLAGS += -lz

# Netlink Library
LDFLAGS += -lnl-tiny

# Add hardware ID this LEDE build is for
CPPFLAGS += -DDEVICE_PRODUCT_NAME=\"$(HARDWARE)\"
