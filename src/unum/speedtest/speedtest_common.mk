# (c) 2019 minim.co
# Common Makefile for the speedtest component

# Add speedtest folder to the include search path
CPPFLAGS += -I$(UNUM_PATH)/speedtest/

# Add code file(s)
OBJECTS += ./speedtest/speedtest.o
