CC = gcc
CFLAGS = -Wall `pkg-config fuse3 --cflags`
LDFLAGS = `pkg-config fuse3 --libs`

SRC = src/main.c src/operations.c src/path.c src/cow.c src/utils.c
OUT = mini_unionfs

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT) $(LDFLAGS)

clean:
	rm -f $(OUT)