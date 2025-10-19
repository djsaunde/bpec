CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g
INCLUDES := -Iinclude

SRCS := $(wildcard src/*.c)
OBJS := $(SRCS:.c=.o)

bpe: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) bpe