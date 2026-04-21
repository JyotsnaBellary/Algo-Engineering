#!/bin/bash

# Stop if any command fails
set -e
rm -rf build

echo "Creating build directory..."
mkdir -p build

echo "Entering build directory..."
cd build

echo "Running CMake..."
cmake ..

echo "Building project..."
make

echo "Running executable..."
./multiwaycut