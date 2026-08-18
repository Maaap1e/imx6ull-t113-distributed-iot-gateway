ifneq ($(KERNELRELEASE),)
obj-m += ap3216c.o
obj-m += icm20608.o
else
KERNEL_DIR ?= /path/to/imx6ull-linux-kernel
ARCH ?= arm
CROSS_COMPILE ?= arm-linux-gnueabihf-
USER_CC ?= $(CROSS_COMPILE)gcc
USER_CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -O2
SMOKE_TEST := sensor_smoke_test

.PHONY: all modules user-test clean clean-user

all: modules

modules:
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

$(SMOKE_TEST): sensor_smoke_test.c sensor_uapi.h
	$(USER_CC) $(USER_CFLAGS) -o $@ sensor_smoke_test.c

user-test: $(SMOKE_TEST)

clean: clean-user
	$(MAKE) -C $(KERNEL_DIR) M=$(CURDIR) \
		ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) clean

clean-user:
	$(RM) $(SMOKE_TEST)
endif
