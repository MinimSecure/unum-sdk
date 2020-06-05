# (c) 2017-2019 minim.co
# Plaform independent part of Makefile for various code tests


# Add model specific include path to make platform specific  
# headers come from the right place.
CPPFLAGS += -I$(UNUM_PATH)/tests/$(MODEL)

# Add common code file(s)
OBJECTS += ./tests/tests.o ./tests/tests_stubs.o ./tests/test_tpacket2.o \
           ./tests/test_crashes.o

# Add model code file(s)
OBJECTS += ./tests/$(MODEL)/tests_platform.o 
