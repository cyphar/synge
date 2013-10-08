#!/usr/bin/env python3

# Synge-GTK: A graphical interface for Synge
# Copyright (c) 2013 Cyphar

# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:

# 1. The above copyright notice and this permission notice shall be included in
#    all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Takes 3 arguments:
# bakeui.py <template> <xml-ui> <output-file>

from sys import argv, exit
from re import sub
from codecs import open

def main():
	if len(argv) != 4:
		exit(1)

	xmlui = ""
	template = ""

	with open(argv[2], "r", "utf8") as f:
		xmlui = f.read().replace('"', '\\"').replace("\n", "")
		xmlui = sub(">\s+?<", "><", xmlui)

	with open(argv[1], "r", "utf8") as f:
		template = f.read()
		final = template.replace("STRING_REPLACED_DURING_COMPILE_TIME", xmlui);

	with open(argv[3], "w", "utf8") as f:
		f.write(final + "\n\n")

if __name__ == "__main__":
	main()
