#!/bin/bash

# Loop through all files in the current directory with a '-TMP' suffix
for file in *-TMP; do
  # Remove the '-TMP' suffix from the file name
  new_file="${file%-TMP}"
  
  # Rename the file
  mv "$file" "$new_file"
done
