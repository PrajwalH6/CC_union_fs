CC      = gcc
CFLAGS  = -Wall -Wextra -g $(shell pkg-config fuse3 --cflags) -Iinclude
LDFLAGS = $(shell pkg-config fuse3 --libs)

SRCS    = src/main.c src/operations.c src/cow.c src/utils.c
OBJS    = $(SRCS:.c=.o)
TARGET  = mini_unionfs

.PHONY: all clean debug

all: $(TARGET)

debug: CFLAGS += -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $

clean:
	rm -f $(OBJS) $(TARGET)