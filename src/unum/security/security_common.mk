# (c) 2019 minim.co
# security subsystem

# Add subsystem folder to the include search path
CPPFLAGS += -I$(UNUM_PATH)/security

# Add code file(s)
OBJECTS += \
  ./security/port_scan.o
