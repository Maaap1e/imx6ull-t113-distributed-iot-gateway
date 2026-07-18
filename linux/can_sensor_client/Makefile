CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
LDFLAGS ?=

TARGET := stm32_can_sensor_client

.PHONY: all clean

all: $(TARGET)

$(TARGET): stm32_can_sensor_client.c
	$(CC) $(CFLAGS) -o $@ stm32_can_sensor_client.c $(LDFLAGS)

clean:
	$(RM) $(TARGET)
