CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -Werror -O2
TARGET := protocol_test

.PHONY: all test clean

all: $(TARGET)

$(TARGET): protocol_test.c ../linux/imx6ull_gateway/protocol.c
	$(CC) $(CFLAGS) -I../linux/imx6ull_gateway -o $@ $^

test: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)
