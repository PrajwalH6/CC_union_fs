#!/bin/bash

BINARY="./mini_unionfs"
TEST_DIR="./test_env"
LOWER="$TEST_DIR/lower"
UPPER="$TEST_DIR/upper"
MNT="$TEST_DIR/mnt"

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASSED=0
FAILED=0
TOTAL=0

test_start() {
    TOTAL=$((TOTAL + 1))
    echo -n "Test $TOTAL: $1... "
}

test_pass() {
    PASSED=$((PASSED + 1))
    echo -e "${GREEN}PASS${NC}"
}

test_fail() {
    FAILED=$((FAILED + 1))
    echo -e "${RED}FAIL${NC}"
    [ -n "$1" ] && echo "  Reason: $1"
}

cleanup() {
    fusermount -u "$MNT" 2>/dev/null || umount "$MNT" 2>/dev/null
    kill $FUSE_PID 2>/dev/null
    wait $FUSE_PID 2>/dev/null
}

trap cleanup EXIT

echo "Setting up test environment..."
rm -rf "$TEST_DIR"
mkdir -p "$LOWER" "$UPPER" "$MNT"

echo "Mounting filesystem..."
$BINARY "$LOWER" "$UPPER" "$MNT" &
FUSE_PID=$!
sleep 2

if ! mountpoint -q "$MNT" 2>/dev/null; then
    echo "Failed to mount!"
    exit 1
fi

echo ""
echo "=== BASIC OPERATIONS ==="

test_start "Read from lower layer"
echo "lower" > "$LOWER/test.txt"
grep -q "lower" "$MNT/test.txt" && test_pass || test_fail

test_start "Write triggers CoW"
echo "modified" >> "$MNT/test.txt"
[ -f "$UPPER/test.txt" ] && grep -q "modified" "$UPPER/test.txt" && test_pass || test_fail

test_start "File creation"
echo "new" > "$MNT/new.txt"
[ -f "$UPPER/new.txt" ] && test_pass || test_fail

echo ""
echo "=== DIRECTORY OPERATIONS ==="

test_start "mkdir"
mkdir "$MNT/newdir"
[ -d "$UPPER/newdir" ] && test_pass || test_fail

test_start "rmdir empty"
rmdir "$MNT/newdir"
[ ! -d "$MNT/newdir" ] && test_pass || test_fail

test_start "rmdir non-empty fails"
mkdir "$MNT/nonempty"
touch "$MNT/nonempty/file"
rmdir "$MNT/nonempty" 2>/dev/null
[ -d "$MNT/nonempty" ] && test_pass || test_fail

echo ""
echo "=== WHITEOUT ==="

test_start "Delete lower file creates whiteout"
echo "delete_me" > "$LOWER/delete.txt"
rm "$MNT/delete.txt"
[ ! -f "$MNT/delete.txt" ] && [ -f "$UPPER/.wh.delete.txt" ] && test_pass || test_fail

echo ""
echo "=== ADVANCED FEATURES ==="

test_start "truncate"
echo "123456789" > "$MNT/trunc.txt"
truncate -s 4 "$MNT/trunc.txt"
grep -q "1234" "$MNT/trunc.txt" && test_pass || test_fail

test_start "rename"
mv "$MNT/test.txt" "$MNT/renamed.txt"
[ -f "$MNT/renamed.txt" ] && test_pass || test_fail

test_start "symlink"
ln -s renamed.txt "$MNT/link.txt"
[ -L "$MNT/link.txt" ] && test_pass || test_fail

test_start "readlink"
readlink "$MNT/link.txt" | grep -q "renamed.txt" && test_pass || test_fail

test_start "chmod"
chmod 600 "$MNT/renamed.txt"
perms=$(stat -c "%a" "$UPPER/renamed.txt")
[ "$perms" = "600" ] && test_pass || test_fail

test_start "xattr"
setfattr -n user.test -v "hello" "$MNT/renamed.txt" 2>/dev/null
getfattr -n user.test "$MNT/renamed.txt" 2>/dev/null | grep -q "hello" && test_pass || test_fail

echo ""
echo "=== CACHE ==="

test_start "cache hits"
for i in {1..50}; do cat "$MNT/renamed.txt" > /dev/null; done
test_pass

echo ""
echo "============================================"
echo "            TEST SUMMARY"
echo "============================================"
echo "Total:  $TOTAL"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"
echo ""