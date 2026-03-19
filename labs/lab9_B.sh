#!/bin/bash
# COMP 8567 - Lab 9 Part B
# Author: Lei Jiang
# Counts number of files with specific extension (or all files) in a directory
# Uses positional parameters as required

# Check if at least directory is provided
if [ $# -eq 0 ]; then
    echo "Usage: $0 <directory> [extension]"
    exit 1
fi

dir=$1
ext=$2

if [ -z "$ext" ]; then
    # Count all regular files (no subdirectories)
    count=$(find "$dir" -maxdepth 1 -type f | wc -l)
    echo "$count"
else
    # Count files with given extension (e.g. .c, .txt, .sh)
    count=$(find "$dir" -maxdepth 1 -name "*$ext" -type f | wc -l)
    echo "$count"
fi
