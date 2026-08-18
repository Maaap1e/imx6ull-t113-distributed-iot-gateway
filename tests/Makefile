CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -O2
PROTOCOL_TARGET := protocol_test
MQTT_TARGET := mqtt_command_test
OTA_PACKAGE_TARGET := ota_package_test
SHA256_TARGET := sha256_test
SENSOR_UAPI_TARGET := sensor_uapi_test
TARGETS := $(PROTOCOL_TARGET) $(MQTT_TARGET) $(OTA_PACKAGE_TARGET) \
	$(SHA256_TARGET) $(SENSOR_UAPI_TARGET)

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

$(SHA256_TARGET): sha256_test.c ../common/crypto/sha256.c
	$(CC) $(CFLAGS) -o $@ $^

$(SENSOR_UAPI_TARGET): sensor_uapi_test.c \
		../linux/kernel_drivers/imx6ull_sensors/sensor_uapi.h
	$(CC) $(CFLAGS) -o $@ sensor_uapi_test.c

test: $(TARGETS)
	./$(PROTOCOL_TARGET)
	./$(MQTT_TARGET)
	./$(OTA_PACKAGE_TARGET)
	./$(SHA256_TARGET)
	./$(SENSOR_UAPI_TARGET)

clean:
	$(RM) $(TARGETS)
