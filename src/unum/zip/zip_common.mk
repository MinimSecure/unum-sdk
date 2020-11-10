# (c) 2020 minim.co
# Plaform independent part of Makefile for unum zip functions library 

CPPFLAGS += -I$(UNUM_PATH)/zip/
# Add code file(s)
# Minizip files
OBJECTS += ./zip/ioapi.o ./zip/zip.o ./zip/zip_test.o
