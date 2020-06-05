# (c) 2017-2019 minim.co
# Common Makefile for unum device telemetry job

# Add telemetry folder to the include search path
CPPFLAGS += -I$(UNUM_PATH)/telemetry

# Add code file(s)
OBJECTS += ./telemetry/telemetry.o \
	./telemetry/meminfo.o \
	./telemetry/cpuinfo.o \
	./telemetry/iptables.o

# Add subsystem initializer function
INITLIST += telemetry_init

