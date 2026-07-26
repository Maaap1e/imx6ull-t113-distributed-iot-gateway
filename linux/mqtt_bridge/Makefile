CC ?= gcc
PAHO_PREFIX ?= /usr/local
CPPFLAGS ?=
CFLAGS ?= -std=gnu99 -Wall -Wextra -Wno-missing-field-initializers -O2
LDFLAGS ?=
LDLIBS ?=

CPPFLAGS += -I$(PAHO_PREFIX)/include
LDFLAGS += -L$(PAHO_PREFIX)/lib -Wl,-rpath,/opt/iot-gateway/lib
LDLIBS += -lpaho-mqtt3c -lpthread -ldl -lrt

TARGET := mqtt_bridge
SOURCES := mqtt_bridge.c mqtt_command.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES) mqtt_command.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(TARGET)
