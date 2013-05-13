#!/usr/bin/python

# takes 3 arguments:
# bakeui.py <template> <xml-ui> <output-file>

from sys import argv, exit
from re import sub

def main():
	if len(argv) != 4:
		exit(1)

	xmlui = open(argv[2], "r").read().replace('"', '\\"').replace("\n", "")
	xmlui = sub(">\s+?<", "><", xmlui)

	template = open(argv[1], "r").read()
	final = template.replace("STRING_REPLACED_DURING_COMPILE_TIME", xmlui);
	output = open(argv[3], "w")
	output.write(final + "\n\n")

if __name__ == "__main__":
	main()
