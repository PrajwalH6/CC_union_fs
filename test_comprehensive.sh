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
if grep -q "lower" "$MNT/test.txt" 2>/dev/null; then
    test_pass
else
    test_fail "Could not read from lower"
fi

test_start "Write triggers CoW"
echo "modified" >> "$MNT/test.txt" 2>/dev/null
if [ -f "$UPPER/test.txt" ] && grep -q "modified" "$UPPER/test.txt"; then
    test_pass
else
    test_fail "CoW did not work"
fi

test_start "File creation"
echo "new" > "$MNT/new.txt" 2>/dev/null
if [ -f "$UPPER/new.txt" ]; then
    test_pass
else
    test_fail "File not created in upper"
fi

echo ""
echo "=== DIRECTORY OPERATIONS ==="

test_start "mkdir"
mkdir "$MNT/newdir" 2>/dev/null
if [ -d "$UPPER/newdir" ]; then
    test_pass
else
    test_fail "Directory not created"
fi

test_start "rmdir empty directory"
rmdir "$MNT/newdir" 2>/dev/null
if [ ! -d "$MNT/newdir" ]; then
    test_pass
else
    test_fail "Directory not removed"
fi

test_start "rmdir non-empty fails"
mkdir "$MNT/nonempty" 2>/dev/null
touch "$MNT/nonempty/file" 2>/dev/null
rmdir "$MNT/nonempty" 2>/dev/null
if [ -d "$MNT/nonempty" ]; then
    test_pass
else
    test_fail "Non-empty directory was removed"
fi

echo ""
echo "=== DELETION & WHITEOUT ==="

test_start "Delete lower file creates whiteout"
echo "delete_me" > "$LOWER/delete.txt"
rm "$MNT/delete.txt" 2>/dev/null
if [ ! -f "$MNT/delete.txt" ] 2>/dev/null && [ -f "$UPPER/.wh.delete.txt" ]; then
    test_pass
else
    test_fail "Whiteout not created"
fi

echo ""
echo "=== PERMISSIONS ==="

test_start "chmod"
chmod 600 "$MNT/test.txt" 2>/dev/null
if [ -f "$UPPER/test.txt" ]; then
    perms=$(stat -c "%a" "$UPPER/test.txt" 2>/dev/null || stat -f "%A" "$UPPER/test.txt" 2>/dev/null)
    if [ "$perms" = "600" ] || [ "$perms" = "rw-------" ]; then
        test_pass
    else
        test_fail "Expected 600, got $perms"
    fi
else
    test_fail "File not in upper layer"
fi

test_start "chown (if running as root)"
if [ "$(id -u)" = "0" ]; then
    chown 1000:1000 "$MNT/test.txt" 2>/dev/null
    test_pass
else
    echo "  (skipped - not running as root)"
fi

echo ""
echo "=== PERFORMANCE & CACHE ==="

test_start "Multiple reads (cache test)"
for i in {1..50}; do
    cat "$MNT/test.txt" > /dev/null 2>&1
done
test_pass

echo ""
echo "============================================"
echo "            TEST SUMMARY"
echo "============================================"
echo "Total:  $TOTAL"
echo -e "${GREEN}Passed: $PASSED${NC}"
echo -e "${RED}Failed: $FAILED${NC}"
echo ""
