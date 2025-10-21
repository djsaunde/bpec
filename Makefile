CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g -pthread
LDFLAGS := -pthread

ifeq ($(DEBUG),1)
CFLAGS += -O0
else
CFLAGS += -O2
endif
INCLUDES := -Iinclude

COMMON_SRCS := \
	src/cli.c \
	src/io.c \
	src/merge_rules.c \
	src/pair_heap.c \
	src/sequence.c \
	src/token.c \
	src/tokenizer_io.c \
	src/train.c \
	src/vocab.c

COMMON_OBJS := $(COMMON_SRCS:.c=.o)

BPE_OBJS := src/main.o $(COMMON_OBJS)
INTERACT_OBJS := src/interact.o $(COMMON_OBJS)

bpe: $(BPE_OBJS)
	$(CC) $(CFLAGS) $(BPE_OBJS) $(LDFLAGS) -o $@

interact: $(INTERACT_OBJS)
	$(CC) $(CFLAGS) $(INTERACT_OBJS) $(LDFLAGS) -o $@

src/%.o: src/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(COMMON_OBJS) src/main.o src/interact.o bpe interact
