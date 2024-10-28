#!/bin/bash

# Start quake3e.x64 in the background
./quake3e.x64 &
# Capture the PID of quake3e.x64
pid=$!

# Check if the process started successfully
if [ -z "$pid" ]; then
    echo "Failed to start quake3e.x64."
    exit 1
fi

# Log CPU usage of the specific process every second
echo "Logging CPU usage for quake3e.x64 (PID: $pid)"
pidstat -p $pid 1 > quake3e_cpu_usage.log &

# Wait for quake3e.x64 to finish
wait $pid

# Calculate average CPU usage once the process ends
echo "Calculating average CPU usage..."

# Extract %CPU column, skip the header lines, and calculate the average
average_cpu=$(awk '/^[0-9]/ { sum += $7; count++ } END { if (count > 0) print sum / count; else print 0 }' quake3e_cpu_usage.log)

# Display the result
echo "Average CPU usage for quake3e.x64 was: ${average_cpu}%"
