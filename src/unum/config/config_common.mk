# (c) 2019 minim.co
# Common Makefile for unum device config management job

# Add iplatform config folder to the include search path 
CPPFLAGS += -I$(UNUM_PATH)/config/$(MODEL)

# Add code file(s)
OBJECTS += ./config/config.o ./config/$(MODEL)/config_platform.o

# Add subsystem initializer function
INITLIST += config_init

