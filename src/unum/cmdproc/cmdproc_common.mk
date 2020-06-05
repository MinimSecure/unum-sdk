# (c) 2017-2019 minim.co
# Plaform independent part of Makefile for command processor


# Add model specific include path to make platform specific
# headers come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/cmdproc/$(MODEL)

# Add common code file(s)
OBJECTS += ./cmdproc/cmdproc.o ./cmdproc/fetch_urls.o ./cmdproc/telnet.o
OBJECTS += ./cmdproc/pingflood.o

# Add model specific code file
OBJECTS += ./cmdproc/$(MODEL)/dev_cmds.o

# Add subsystem initializer function
INITLIST += cmdproc_init
