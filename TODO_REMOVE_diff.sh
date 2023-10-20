#!/bin/bash

# Check if two arguments are provided
if [ "$#" -ne 2 ]; then
    echo "Usage: ./diff_folders.sh <folder1> <folder2>"
    exit 1
fi

# Assign arguments to variables
FOLDER1=$1
FOLDER2=$2

# Check if the folders exist
if [ ! -d "$FOLDER1" ]; then
    echo "Folder $FOLDER1 does not exist."
    exit 1
fi

if [ ! -d "$FOLDER2" ]; then
    echo "Folder $FOLDER2 does not exist."
    exit 1
fi

# Loop through all files in the first folder
for file in "$FOLDER1"/*; do
    # Extract just the filename without the folder path
    filename=$(basename "$file")

    # Check if the file exists in the second folder
    if [ -f "$FOLDER2/$filename" ]; then
        # Run the diff command and print differences if they exist
        diff_output=$(diff "$FOLDER1/$filename" "$FOLDER2/$filename")
        if [ ! -z "$diff_output" ]; then
            echo "Differences found in file $filename:"
            echo "$diff_output"
        else
            echo "No differences in $filename."
        fi
    else
        echo "!!!!!!!!!!File $filename does not exist in $FOLDER2.!!!!!!!!!!"
    fi
done
