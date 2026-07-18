CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
LDFLAGS ?=

TARGET := imx6ull_gateway_app
COMMON_SRC := protocol.c sensors.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c $(COMMON_SRC) protocol.h sensors.h
	$(CC) $(CFLAGS) -o $@ main.c $(COMMON_SRC) $(LDFLAGS)

clean:
	$(RM) $(TARGET)
