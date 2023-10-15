#!/bin/bash

# Check if there is a directory path provided, otherwise use the current directory
directory="${1:-.}"

# Loop through all files in the specified directory
for file in "$directory"/*; do
    if [ -f "$file" ]; then
        filename=$(basename -- "$file")
        new_filename="${filename}-TMP"

        if [ "$filename" = ".hi" ]; then
            new_filename=".hi-TMP"
        fi

        mv "$file" "$directory/$new_filename"
    fi
done
