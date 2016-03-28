#!/usr/bin/env python3

# Synge-GTK: A graphical interface for Synge
# Copyright (C) 2013, 2016 Aleksa Sarai

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

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
