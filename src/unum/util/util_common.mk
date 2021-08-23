# (c) 2017-2020 minim.co
# Plaform independent part of Makefile for unum utility functions library 

# Add code file(s)
OBJECTS += ./util/util.o ./util/jobs.o ./util/util_event.o ./util/util_net.o
OBJECTS += ./util/util_json.o ./util/util_timer.o ./util/util_crashinfo.o
OBJECTS += ./util/$(MODEL)/util_platform.o ./util/util_stubs.o ./util/util_dns.o
OBJECTS += ./util/util_kind.o

# Add zlib files
OBJECTS += ./util/util_zlib.o

# Add C99 Code Files
XOBJECTS = ./util/dns.o

# Add subsystem initializer function
INITLIST += util_init

