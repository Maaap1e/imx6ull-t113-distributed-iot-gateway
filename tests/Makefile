CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -O2
PROTOCOL_TARGET := protocol_test
MQTT_TARGET := mqtt_command_test
TARGETS := $(PROTOCOL_TARGET) $(MQTT_TARGET)

.PHONY: all test clean

all: $(TARGETS)

$(PROTOCOL_TARGET): protocol_test.c ../linux/imx6ull_gateway/protocol.c
	$(CC) $(CFLAGS) -I../linux/imx6ull_gateway -o $@ $^

$(MQTT_TARGET): mqtt_command_test.c ../linux/mqtt_bridge/mqtt_command.c
	$(CC) $(CFLAGS) -I../linux/mqtt_bridge -o $@ $^

test: $(TARGETS)
	./$(PROTOCOL_TARGET)
	./$(MQTT_TARGET)

clean:
	$(RM) $(TARGETS)
