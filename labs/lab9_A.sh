#!/bin/bash
# COMP 8567 - Lab 9 Part A
# This script performs operations on sample.txt based on write permission and file size

file="sample.txt"

# Check if user has write permission on sample.txt
if [ -w "$file" ]; then
    # Check if file size > 0
    if [ -s "$file" ]; then
        echo "The size of sample.txt > 0, the output of ls will now be appended" >> "$file"
        ls >> "$file"
    else
        echo "The size of sample.txt = 0, the output of ls will now be newly added" >> "$file"
        ls > "$file"
    fi
else
    # No write permission - check if current user is the owner
    if [ -O "$file" ]; then
        chmod u+w "$file"
        echo "Write permission has been granted" >> "$file"
    else
        echo "You are not the owner of the file" >> "$file"
    fi
fi
