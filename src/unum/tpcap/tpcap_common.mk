# (c) 2019 minim.co
# Common Makefile for packet capturing job

# Add platform folder to the include search path 
CPPFLAGS += -I$(UNUM_PATH)/tpcap/$(MODEL)

# Add code file(s)
OBJECTS += \
  ./tpcap/tpcap.o       \
  ./tpcap/process_pkt.o \
  ./tpcap/$(MODEL)/tpcap_platform.o

# Add subsystem initializer function
INITLIST += tpcap_init

