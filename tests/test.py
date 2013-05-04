#!/usr/bin/python -tt

# Synge-TestSuite: The "official" test suite for Synge
# Copyright (C) 2013 Cyphar

# Permission is hereby granted, free of charge, to any person obtaining a copy of
# this software and associated documentation files (the "Software"), to deal in
# the Software without restriction, including without limitation the rights to
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
# the Software, and to permit persons to whom the Software is furnished to do so,
# subject to the following conditions:

# 1. The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

from sys import argv
from commands import *


deg = "degrees"
rad = "radians"

ansi_error = "\x1b[1;31m"
ansi_good  = "\x1b[1;32m"
ansi_reset = "\x1b[0m"

errors = {
		"zerodiv"	: "Attempted to divide or modulo by zero.",
		"parens"	: "Missing parenthesis in expression.",
		"token"		: "Unknown token or function in expression.",
		"numvals"	: "Incorrect number of values for operator or function.",
		"empty"		: "Expression was empty.",
		"unknown"	: "An unknown error has occured."
}

# List of case tuples

CASES = [
#	test expression			expected result		mode	test description

	# expected successes
	("1+1",				"2",			deg,	"Basic Addition		"),
	("1+-1",			"0",			deg,	"Convoluted Addition	"),
	("5-3",				"2",			deg,	"Basic Subtraction	"),
	("3*4",				"12",			deg,	"Basic Multiplication	"),
	("4/2",				"2",			deg,	"Basic Division		"),
	("5%2",				"1",			deg,	"Basic Modulo		"),
	("15.1%2",			"1.0999999999999996",	deg,	"Basic Modulo		"), # needs to be fixed (due to rounding err)
	("(1+1)*2",			"4",			deg,	"Basic Parenthesis	"),
	("10^2",			"100",			deg,	"Basic Indicies		"),
	("16^(1/4)",			"2",			deg,	"Fractional Indicies	"),

	("0xDEADBEEF + 0xA",		"3735928569",		deg,	"Hexadecimal Addition	"),
	("0xDEADBEEF - 0xA",		"3735928549",		deg,	"Hexadecimal Subtraction	"),
	("0xA0 / 0xA",			"16",			deg,	"Hexadecimal Division	"),

	("0xDEADBEEF + 10",		"3735928569",		deg,	"Mixed Addition		"),
	("0xDEADBEEF - 10",		"3735928549",		deg,	"Mixed Subtraction	"),
	("0xA0 / 10",			"16",			deg,	"Mixed Division		"),

	("3.0+2.1",			"5.0999999999999996",	deg,	"Basic Decimal Notation	"), # needs to be fixed (due to rounding err)
	("5.3+2",			"7.2999999999999998",	deg,	"Basic Decimal Notation	"), # needs to be fixed (due to rounding err)
	("0.1+2",			"2.1",			deg,	"Basic Decimal Notation	"),
	("1.0+3",			"4",			deg,	"Basic Decimal Notation	"),
	(".1+2",			"2.1",			deg,	"Basic Decimal Notation	"),
	("1.+3",			"4",			deg,	"Basic Decimal Notation	"),

	("log10(100)/2",		"1",			deg,	"Basic Function Division	"),

	("tan(45)+cos(60)+sin(30)",	"2",			deg,	"Basic Degrees Trig	"),
	("atan(1)+acos(0.5)+asin(0)",	"105",			deg,	"Basic Degrees Trig	"),

	("tan(45)+cos(60)+sin(30)",	"-0.320669413964157",	rad,	"Basic Radian Trig	"),
	("atan(1)+acos(0.5)+asin(0)",	"1.832595714594046",	rad,	"Basic Radian Trig	"),

	("deg2rad(180/pi)+rad2deg(pi)",	"181",			deg,	"Basic Angle Conversion	"),

	("2^2^2-15-14-12-11-10^5",	"-100036",		deg,	"Complex Expression	"),

	# expected errors
	("",				errors["empty"],	deg,	"Empty Expression Error	"),
	(" ",				errors["empty"],	deg,	"Empty Expression Error	"),
	("does_not_exist",		errors["token"],	deg,	"Unknown Token Error	"),
	("fake_function()",		errors["token"],	deg,	"Unknown Token Error	"),
	("fake_function()+23",		errors["token"],	deg,	"Unknown Token Error	"),
	("1@5",				errors["token"],	deg,	"Unknown Token Error	"),
	("1/0",				errors["zerodiv"],	deg,	"Zero Division Error	"),
	("1%(2-(2^2/2))",		errors["zerodiv"],	deg,	"Modulo by Zero Error	"),
	("1+(1",			errors["parens"],	deg,	"Parenthesis Error	"),
	("1+4)",			errors["parens"],	deg,	"Parenthesis Error	"),
	("1+-+4",			errors["numvals"],	deg,	"Token Number Error	"),
	("2+1-",			errors["numvals"],	deg,	"Token Number Error	"),
	("abs()",			errors["numvals"],	deg,	"Token Number Error	"),
]

def test_calc(program, test, expected, mode, description):
	command = '%s -m %s "%s"' % (program, mode, test)
	output = getstatusoutput(command)[1]
	if output.replace("\n", "") == expected:
		print "%s ... %sOK%s" % (description, ansi_good, ansi_reset)
		return True
	else:
		print "%s ... %sFAIL%s" % (description, ansi_error, ansi_reset)
		print '\tExpression: "%s"' % (test)
		print '\tOutput: "%s%s%s"' % (ansi_error, output, ansi_reset)
		print '\tExpected: "%s%s%s"' % (ansi_good, expected, ansi_reset)
		return False

def run_tests(program):
	failures = 0
	counter = 0
	for case in CASES:
		counter += 1
		if not test_calc(program, case[0], case[1], case[2], case[3]):
			failures += 1
	print "--- Test Summary ---"
	print "Cases run: %d" % (counter)
	if failures > 0:
		print "Failed cases: %d" % (failures)
	else:
		print "All tests passed"

def main():
	print "--- Beginning Synge Test Suite ---"
	if len(argv) < 2:
		print "Error: no test executable given"
		return 1
	run_tests(argv[1])
	return 0

if __name__ == "__main__":
	exit(main())
