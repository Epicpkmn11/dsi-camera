#!/bin/bash

# Function to display a simple loading bar
show_loading() {
    local pid=$1
    local delay=0.1
    local spin='-\|/'
    local i=0
    while kill -0 $pid 2>/dev/null; do
        i=$(( (i+1) %4 ))
        printf "\r[%c] Working..." "${spin:$i:1}"
        sleep $delay
    done
    printf "\r[✔] Done!          \n"
}

# Check if the user provided the .bin file path and framerate
if [ -z "$1" ] || [ -z "$2" ]; then
    echo "Usage: $0 /path/to/file.bin <output_framerate>"
    exit 1
fi

# Assign input arguments
input_file="$1"
output_framerate="$2"

# Run the Python script with the provided .bin file (suppress its output)
echo "Converting .bin file to PNG images..."
python3 convert.py "$input_file" > /dev/null 2>&1 &
show_loading $!

# Check if the Python script succeeded
if [ $? -ne 0 ]; then
    echo "Error: Python script failed. Check the input file and try again."
    exit 1
fi

# Get the directory where the script is located
script_dir=$(dirname "$(readlink -f "$0")")

# Define the output directory
out_dir="$script_dir/out"

# Check if the output directory exists and contains PNG files
if [ ! -d "$out_dir" ]; then
    echo "Error: Output directory 'out' not found."
    exit 1
fi

if [ -z "$(ls -A "$out_dir"/*.png 2>/dev/null)" ]; then
    echo "Error: No PNG files found in the output directory."
    exit 1
fi

# Use ffmpeg to stitch the PNG files into a video with the specified framerate
echo "Stitching PNG images into a video at ${output_framerate}fps..."
ffmpeg -y -r 5 -i "$out_dir/%04d.png" -c:v libx264 -vf "fps=${output_framerate},format=yuv420p" "$script_dir/out.mp4" > /dev/null 2>&1 &
show_loading $!

# Check if ffmpeg succeeded
if [ $? -eq 0 ]; then
    echo "Video successfully created: $script_dir/out.mp4"
else
    echo "Error: ffmpeg failed to create the video."
    exit 1
fi
