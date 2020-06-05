# (c) 2019 minim.co
# Device fingerprinting information collection and reporting

# Add platform folder to the include search path
CPPFLAGS += -I$(UNUM_PATH)/fingerprint/$(MODEL)

# Add code file(s)
OBJECTS += \
  ./fingerprint/fp_main.o \
  ./fingerprint/fp_ssdp.o \
  ./fingerprint/fp_dhcp.o \
  ./fingerprint/fp_mdns.o \
  ./fingerprint/fp_useragent.o

# Add subsystem initializer function
INITLIST += fp_init
