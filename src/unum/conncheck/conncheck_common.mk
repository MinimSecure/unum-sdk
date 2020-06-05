# (c) 2019 minim.co
# Common Makefile for unum connectivity checker job

# Add model specific include path to make conncheck.h 
# come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/conncheck/$(MODEL)

# Add code file(s)
OBJECTS += ./conncheck/conncheck.o

# Add subsystem initializer function
INITLIST += conncheck_init

