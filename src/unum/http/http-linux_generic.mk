# (c) 2018 minim.co
# Plaform independet Makefile for curl-based http functions library 

# Add model specific include path to make http.h 
# come from the right place
CPPFLAGS += -I$(UNUM_PATH)/http/$(MODEL)

# Add code file(s)
OBJECTS += ./http/http_curl.o

# Add subsystem initializer function
INITLIST += http_init
