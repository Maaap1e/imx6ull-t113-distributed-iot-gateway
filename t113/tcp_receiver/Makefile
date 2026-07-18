CC ?= gcc
CFLAGS ?= -std=gnu99 -Wall -Wextra -O2
LDFLAGS ?=

TARGET := t113_display_app
COMMON_SRC := protocol.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): main.c $(COMMON_SRC) protocol.h
	$(CC) $(CFLAGS) -o $@ main.c $(COMMON_SRC) $(LDFLAGS)

clean:
	$(RM) $(TARGET)
