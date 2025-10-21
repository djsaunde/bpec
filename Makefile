CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -pthread
LDFLAGS := -pthread
INCLUDES := -Iinclude

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

bpe: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) bpe
