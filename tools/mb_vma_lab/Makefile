CC ?= gcc
CFLAGS ?= -g

all: user_addr

user_addr: user_addr.c
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@

.PHONY: all clean
clean:
	rm -f user_addr *.o
