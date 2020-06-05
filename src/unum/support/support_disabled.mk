# (c) 2019 minim.co
# Plaform independent part of Makefile for support portal agent

# Add model specific include path to make support.h 
# come from the right place, but do not compile or add
# anything else since the feature is not enabled for the platform
CPPFLAGS += -I$(UNUM_PATH)/support/$(MODEL)

