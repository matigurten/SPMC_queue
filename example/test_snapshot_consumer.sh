#!/bin/bash

# Test script for snapshot consumers
# This shows the correct way to run the reader and consumers

echo "=== SPMC Queue Snapshot Consumer Test ==="
echo ""

# Clean up any existing shared memory
echo "Cleaning up existing shared memory..."
rm -f /dev/shm/myqueue
rm -f /dev/shm/tob_snapshots
rm -f /dev/shm/fod_snapshots

echo ""

# Start the writer in background
echo "Starting writer..."
./shm_writer myqueue &
WRITER_PID=$!

# Wait a moment for writer to start
sleep 1

# Start the reader with correct arguments (3 arguments needed)
echo "Starting reader with TOB and FOD shared memory..."
./shm_reader myqueue /tob_snapshots /fod_snapshots &
READER_PID=$!

# Wait for reader to create shared memory
sleep 2

echo ""

# Start TOB consumer
echo "Starting TOB consumer..."
./snapshot_consumer tob /tob_snapshots consumer1 &
TOB_CONSUMER_PID=$!

# Start FOD consumer
echo "Starting FOD consumer..."
./snapshot_consumer fod /fod_snapshots consumer2 4 &
FOD_CONSUMER_PID=$!

echo ""
echo "All components started. Press Ctrl+C to stop."
echo ""

# Wait for user to stop
trap "echo 'Stopping all processes...'; kill $WRITER_PID $READER_PID $TOB_CONSUMER_PID $FOD_CONSUMER_PID 2>/dev/null; exit" INT

# Keep running until interrupted
while true; do
    sleep 1
done 