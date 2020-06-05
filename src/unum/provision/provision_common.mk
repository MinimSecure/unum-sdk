# (c) 2017-2019 minim.co
# Common Makefile for unum device provision job

# Add model specific include path to make provision.h 
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/provision/$(MODEL)

# Add code file(s)
OBJECTS += ./provision/provision.o

# Add subsystem initializer function
INITLIST += provision_init

