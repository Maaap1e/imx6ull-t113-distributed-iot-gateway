CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
LDFLAGS ?=

TARGET := stm32_can_ota_host

.PHONY: all clean

all: $(TARGET)

$(TARGET): stm32_can_ota_host.c
	$(CC) $(CFLAGS) -o $@ stm32_can_ota_host.c $(LDFLAGS)

clean:
	$(RM) $(TARGET)
