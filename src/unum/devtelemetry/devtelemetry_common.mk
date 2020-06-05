# (c) 2017-2019 minim.co
# Device telemetry information collection and reporting

# Add platform folder to the include search path 
CPPFLAGS += -I$(UNUM_PATH)/devtelemetry/$(MODEL)

# Add code file(s)
OBJECTS += \
  ./devtelemetry/dt_collector.o  \
  ./devtelemetry/dns_collector.o \
  ./devtelemetry/dt_sender.o     \
  ./devtelemetry/dt_stubs.o

# Add subsystem initializer function
INITLIST += devtelemetry_init

