CC     = gcc
SRCS   = src/main.c src/operations.c src/cow.c src/utils.c
OBJS   = $(SRCS:.c=.o)
TARGET = mini_unionfs

# ── Platform detection ──────────────────────────────────────────
UNAME := $(shell uname)

ifeq ($(UNAME), Darwin)
  # macOS — macFUSE
  FUSE_CFLAGS = -I/Library/Frameworks/macFUSE.framework/Versions/A/Headers
  FUSE_LIBS   = -L/Library/Frameworks/macFUSE.framework/Versions/A -lfuse -lpthread
else
  # Linux / Windows WSL — FUSE3
  FUSE_CFLAGS = $(shell pkg-config fuse3 --cflags)
  FUSE_LIBS   = $(shell pkg-config fuse3 --libs)
endif

CFLAGS  = -Wall -Wextra -g -D_FILE_OFFSET_BITS=64 $(FUSE_CFLAGS) -Iinclude
LDFLAGS = $(FUSE_LIBS)

# ── Targets ─────────────────────────────────────────────────────
.PHONY: all clean debug

all: $(TARGET)

debug: CFLAGS += -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)