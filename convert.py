#!/usr/bin/env python3

from os import mkdir, path
from struct import pack, unpack
from PIL import Image
from sys import argv

# Make a video with:
# ffmpeg -y -r 5 -i out/%04d.png out.mp4

if len(argv) < 2:
	print("Please give the path to the file.")
	exit(1)

with open(argv[1], "rb") as f:
	if not path.exists("out"):
		mkdir("out")

	i = 0
	while True:
		buf = f.read(256 * 192 * 2)

		if len(buf) != 256 * 192 * 2:
			break

		rgb = b""

		for j in range(0, 256 * 192 * 2, 2):
			val, = unpack("<H", buf[j:j + 2])
			r = (val & 0x1F) << 3
			g = ((val >> 5) & 0x1F) << 3
			b = ((val >> 10) & 0x1F) << 3
			rgb += pack("BBB", r, g, b)

		im = Image.new("RGB", (256, 192))
		im.frombytes(rgb)

		im.save(path.join("out", "%04d.png" % i))

		delay, = unpack("<I", f.read(4))
		print(delay)

		i += 1
