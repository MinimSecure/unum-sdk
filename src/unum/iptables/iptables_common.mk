# (c) 2017-2021 minim.co
# Common Makefile for unum iptables telemetry job

# Add telemetry folder to the include search path
CPPFLAGS += -I$(UNUM_PATH)/iptables

# Add code file(s)
OBJECTS += ./iptables/iptables.o \
	./iptables/rules.o

# Add subsystem initializer function
INITLIST += iptables_init

