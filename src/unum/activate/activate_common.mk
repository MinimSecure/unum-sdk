# (c) 2017-2019 minim.co
# Common Makefile for unum device activate job

# Add activate folder to the include search path
CPPFLAGS += -I$(UNUM_PATH)/activate/$(MODEL)

# Add code file(s)
OBJECTS += ./activate/activate.o

# Add subsystem initializer function
INITLIST += activate_init
