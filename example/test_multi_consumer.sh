#!/bin/bash

echo "Testing Multi-Consumer Snapshot System"
echo "======================================"

# Clean up any existing shared memory
echo "Cleaning up existing shared memory..."
rm -f /dev/shm/myqueue*
rm -f /dev/shm/snapshots*

# Build the programs
echo "Building programs..."
./build.sh

# Start the writer in background
echo "Starting writer..."
./shm_writer myqueue &
WRITER_PID=$!

# Wait a moment for writer to start
sleep 1

# Start the reader in background
echo "Starting reader..."
./shm_reader myqueue /snapshots &
READER_PID=$!

# Wait a moment for reader to start
sleep 2

# Start multiple consumers
echo "Starting consumers..."
./snapshot_consumer /snapshots consumer1 &
CONSUMER1_PID=$!

./snapshot_consumer /snapshots consumer2 &
CONSUMER2_PID=$!

./snapshot_monitor /snapshots &
MONITOR_PID=$!

echo "All processes started:"
echo "  Writer PID: $WRITER_PID"
echo "  Reader PID: $READER_PID"
echo "  Consumer1 PID: $CONSUMER1_PID"
echo "  Consumer2 PID: $CONSUMER2_PID"
echo "  Monitor PID: $MONITOR_PID"

echo ""
echo "System is running. Press Enter to stop all processes..."
read

# Stop all processes
echo "Stopping all processes..."
kill $WRITER_PID 2>/dev/null
kill $READER_PID 2>/dev/null
kill $CONSUMER1_PID 2>/dev/null
kill $CONSUMER2_PID 2>/dev/null
kill $MONITOR_PID 2>/dev/null

# Wait for processes to stop
sleep 2

# Clean up
echo "Cleaning up shared memory..."
rm -f /dev/shm/myqueue*
rm -f /dev/shm/snapshots*

echo "Test completed." 