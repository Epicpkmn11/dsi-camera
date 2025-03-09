#!/usr/bin/env python3

import os
import sys
from struct import pack, unpack
from PIL import Image

# Check if the user provided a file path as a command-line argument
if len(sys.argv) != 2:
    print("Usage: python convert.py /path/to/file.bin")
    exit(1)

# Get the path to the .bin file from the command-line argument
input_file_path = sys.argv[1]

# Get the directory where the script is located
script_dir = os.path.dirname(os.path.abspath(__file__))

# Define the output directory path
out_dir = os.path.join(script_dir, "out")

# Create the output directory if it doesn't exist
if not os.path.exists(out_dir):
    os.makedirs(out_dir)

# Check if the file exists
if not os.path.exists(input_file_path):
    print(f"Error: The file '{input_file_path}' does not exist.")
    exit(1)

with open(input_file_path, "rb") as f:
    i = 0
    while True:
        buf = f.read(256 * 192 * 2)

        if len(buf) != 256 * 192 * 2:
            break

        rgb = b""

        for j in range(0, 256 * 192 * 2, 2):
            val, = unpack("<H", buf[j:j+2])
            r = (val & 0x1F) << 3
            g = ((val >> 5) & 0x1F) << 3
            b = ((val >> 10) & 0x1F) << 3
            rgb += pack("BBB", r, g, b)

        im = Image.new("RGB", (256, 192))
        im.frombytes(rgb)
        
        # Save the image in the output directory
        im.save(os.path.join(out_dir, f"{i:04d}.png"))

        delay, = unpack("<I", f.read(4))
        print(delay)

        i += 1
