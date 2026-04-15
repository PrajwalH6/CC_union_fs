#!/bin/bash

TEST_DIR="./bench_env"
LOWER="$TEST_DIR/lower"
UPPER="$TEST_DIR/upper"
MNT="$TEST_DIR/mnt"

BINARY="./mini_unionfs"

echo "=== UnionFS Performance Benchmark ==="
echo ""

# Cleanup function
cleanup() {
    fusermount -u "$MNT" 2>/dev/null || umount "$MNT" 2>/dev/null
    kill $FUSE_PID 2>/dev/null
    wait $FUSE_PID 2>/dev/null
}
trap cleanup EXIT

# Setup
rm -rf "$TEST_DIR"
mkdir -p "$LOWER" "$UPPER" "$MNT"

echo "Mounting filesystem..."
$BINARY "$LOWER" "$UPPER" "$MNT" &
FUSE_PID=$!
sleep 2

# Check mount success
if ! mountpoint -q "$MNT" 2>/dev/null; then
    echo "Mount failed!"
    exit 1
fi

# ----------------------------
# Test 1: Sequential reads
# ----------------------------
echo "Test 1: Sequential reads (1000 iterations)"
echo "data" > "$MNT/test.dat"

start=$(date +%s%N 2>/dev/null || date +%s000000000)
for i in {1..1000}; do
    cat "$MNT/test.dat" > /dev/null 2>&1
done
end=$(date +%s%N 2>/dev/null || date +%s000000000)

elapsed=$((end - start))
avg=$((elapsed / 1000))

echo "  Total: ${elapsed} ns"
echo "  Avg/read: ${avg} ns"
echo ""

# ----------------------------
# Test 2: Random access
# ----------------------------
echo "Test 2: Random access (100 files)"

for i in {1..100}; do
    echo "file $i" > "$MNT/file$i.txt"
done

start=$(date +%s%N 2>/dev/null || date +%s000000000)
for i in {1..100}; do
    r=$((RANDOM % 100 + 1))
    cat "$MNT/file$r.txt" > /dev/null 2>&1
done
end=$(date +%s%N 2>/dev/null || date +%s000000000)

elapsed=$((end - start))
avg=$((elapsed / 100))

echo "  Total: ${elapsed} ns"
echo "  Avg/access: ${avg} ns"
echo ""

# ----------------------------
# Test 3: CoW operations
# ----------------------------
echo "Test 3: CoW operations (50 files)"

rm -rf "$LOWER"/* "$UPPER"/*

for i in {1..50}; do
    echo "original $i" > "$LOWER/cow_file$i.txt"
done

start=$(date +%s%N 2>/dev/null || date +%s000000000)
for i in {1..50}; do
    echo "modified" >> "$MNT/cow_file$i.txt"
done
end=$(date +%s%N 2>/dev/null || date +%s000000000)

elapsed=$((end - start))
avg=$((elapsed / 50))

echo "  Total: ${elapsed} ns"
echo "  Avg/CoW: ${avg} ns"
echo ""

echo "Benchmark completed!"
