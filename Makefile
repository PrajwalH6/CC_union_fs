CC      = gcc
CFLAGS  = $(shell pkg-config --cflags fuse3) -Wall -Wextra -g
LIBS    = $(shell pkg-config --libs fuse3)
TARGET  = mini_unionfs
SRC     = mini_unionfs.c

# ============================================================
# BUILD
# ============================================================

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)

# ============================================================
# QUICK TEST (mirrors test_unionfs.sh from the spec)
# ============================================================

TEST_DIR  = ./unionfs_test_env
LOWER_DIR = $(TEST_DIR)/lower
UPPER_DIR = $(TEST_DIR)/upper
MOUNT_DIR = $(TEST_DIR)/mnt

test: $(TARGET)
	@echo "--- Setting up test environment ---"
	rm -rf $(TEST_DIR)
	mkdir -p $(LOWER_DIR) $(UPPER_DIR) $(MOUNT_DIR)
	echo "base_only_content"  > $(LOWER_DIR)/base.txt
	echo "to_be_deleted"      > $(LOWER_DIR)/delete_me.txt
	./$(TARGET) $(LOWER_DIR) $(UPPER_DIR) $(MOUNT_DIR)
	sleep 1

	@echo ""
	@echo "--- Test 1: Layer Visibility ---"
	@grep -q "base_only_content" $(MOUNT_DIR)/base.txt \
		&& echo "PASSED" || echo "FAILED"

	@echo "--- Test 2: Copy-on-Write ---"
	@echo "modified_content" >> $(MOUNT_DIR)/base.txt
	@[ $$(grep -c "modified_content" $(UPPER_DIR)/base.txt 2>/dev/null) -eq 1 ] \
		&& [ $$(grep -c "modified_content" $(LOWER_DIR)/base.txt 2>/dev/null) -eq 0 ] \
		&& echo "PASSED" || echo "FAILED"

	@echo "--- Test 3: Whiteout ---"
	@rm $(MOUNT_DIR)/delete_me.txt 2>/dev/null; \
	[ ! -f $(MOUNT_DIR)/delete_me.txt ] \
		&& [ -f $(LOWER_DIR)/delete_me.txt ] \
		&& [ -f $(UPPER_DIR)/.wh.delete_me.txt ] \
		&& echo "PASSED" || echo "FAILED"

	@echo ""
	@echo "--- Teardown ---"
	fusermount -u $(MOUNT_DIR) 2>/dev/null || umount $(MOUNT_DIR) 2>/dev/null
	rm -rf $(TEST_DIR)

.PHONY: all clean test

# ============================================================
# FEATURE STATUS  (reflects git log as of latest push)
# ============================================================
#
# --- commit 378d370 : base project ---
#   [x] unionfs_getattr()    — resolve + lstat, works correctly
#   [x] unionfs_read()       — resolve + pread, works correctly
#
# --- commits db3a8d7 / 053abc5 / ff1c3aa : teammate ---
#   [x] unionfs_create()     — creates new files in upper layer
#   [x] unionfs_mkdir()      — creates directories in upper layer
#   [x] unionfs_rmdir()      — removes directories from upper layer
#                              NOTE: still needs whiteout for lower dirs
#
# --- commit f9a2c10 : teammate ---
#   [x] unionfs_readdir()    — upper+lower merge with deduplication
#                              NOTE: whiteout check passes filename not
#                              full path — needs fix (get_whiteout_path bug)
#                              NOTE: seen[256][256] overflows for large dirs
#
# --- commits 4a6fcba / 117ccad / 2439640 : omkar ---
#   [x] ensure_upper_dirs()  — mkdir -p parent dirs before copy
#   [x] copy_to_upper()      — preserves permissions, O_TRUNC on dst
#   [x] unionfs_open()       — early CoW trigger on write-mode open
#   [x] unionfs_write()      — O_CREAT fallback, correct errno return
#
# ============================================================
# PENDING
# ============================================================
#
# Person 1 — Core fixes:
#   [ ] get_whiteout_path()  — drops parent dir for nested paths
#                              e.g. /subdir/file.txt creates upper/.wh.file.txt
#                              instead of upper/subdir/.wh.file.txt
#   [ ] unionfs_readdir()    — pass full path (not just d_name) to
#                              get_whiteout_path() for correct whiteout check
#   [ ] seen[256][256]       — replace with heap allocation to fix
#                              stack overflow for dirs with 256+ entries
#
# Person 4 — Deletion & rename:
#   [ ] unionfs_unlink()     — whiteout path wrong for nested files
#                              (blocked on get_whiteout_path fix)
#   [ ] unionfs_rmdir()      — must create whiteout for lower-only dirs
#                              currently only removes from upper
#   [ ] unionfs_rename()     — not implemented, returns ENOSYS
#   [ ] unionfs_truncate()   — not implemented, needed for echo > file
#                              style writes (shell truncates before writing)
#
# Shared:
#   [ ] Makefile             — this file, to be committed by Person 1
#   [ ] Design document      — 2-3 pages, one section per member
# ============================================================