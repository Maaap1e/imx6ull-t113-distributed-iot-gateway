CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
LDFLAGS ?=
LDLIBS ?= -lrt

TARGET := stm32_can_ota_ab_secure_host

.PHONY: all clean

all: $(TARGET)

$(TARGET): stm32_can_ota_host.c ota_package.c ota_package.h \
	../../common/ota_contract_ab.h ../../common/crypto/sha256.c
	$(CC) $(CFLAGS) -o $@ stm32_can_ota_host.c ota_package.c \
		../../common/crypto/sha256.c $(LDFLAGS) $(LDLIBS)

clean:
	$(RM) $(TARGET)
