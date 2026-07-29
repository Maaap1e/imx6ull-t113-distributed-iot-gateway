CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -O2
PROTOCOL_TARGET := protocol_test
MQTT_TARGET := mqtt_command_test
OTA_PACKAGE_TARGET := ota_package_test
TARGETS := $(PROTOCOL_TARGET) $(MQTT_TARGET) $(OTA_PACKAGE_TARGET)

.PHONY: all test clean

all: $(TARGETS)

$(PROTOCOL_TARGET): protocol_test.c ../linux/imx6ull_gateway/protocol.c
	$(CC) $(CFLAGS) -I../linux/imx6ull_gateway -o $@ $^

$(MQTT_TARGET): mqtt_command_test.c ../linux/mqtt_bridge/mqtt_command.c
	$(CC) $(CFLAGS) -I../linux/mqtt_bridge -o $@ $^

$(OTA_PACKAGE_TARGET): ota_package_test.c ../linux/can_ota_host/ota_package.c \
		../linux/can_ota_host/ota_package.h ../common/ota_contract.h
	$(CC) $(CFLAGS) -I../linux/can_ota_host -o $@ \
		ota_package_test.c ../linux/can_ota_host/ota_package.c

test: $(TARGETS)
	./$(PROTOCOL_TARGET)
	./$(MQTT_TARGET)
	./$(OTA_PACKAGE_TARGET)

clean:
	$(RM) $(TARGETS)
