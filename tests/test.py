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
	("(1+1)*2",			"4",			deg,	"Basic Parenthesis	"),

	("3.0+2.1",			"5.0999999999999996",	deg,	"Basic Decimal Notation	"),
	("5.3+2",			"7.2999999999999998",	deg,	"Basic Decimal Notation	"),
	("0.1+2",			"2.1",			deg,	"Basic Decimal Notation	"),
	(".1+2",			"2.1",			deg,	"Basic Decimal Notation	"),
	("1.+3",			"4",			deg,	"Basic Decimal Notation	"),
	("1.0+3",			"4",			deg,	"Basic Decimal Notation	"),

	("tan(45)",			"1",			deg,	"Basic Degrees Trig	"),
	("tan(45)",			"1.6197751905438615",	rad,	"Basic Radian Trig	"),

	("2^2^2-15-14-12-11-10^5",	"-100036",		deg,	"Complex Expression	"),

	# expected errors
	("",				errors["empty"],	deg,	"Empty Expression Error	"),
	(" ",				errors["empty"],	deg,	"Empty Expression Error	"),
	("1/0",				errors["zerodiv"],	deg,	"Zero Division Error	"),
	("1%(2-(2^2/2))",		errors["zerodiv"],	deg,	"Modulo by Zero Error	"),
	("1+(1",			errors["parens"],	deg,	"Parenthesis Error	"),
	("1+4)",			errors["parens"],	deg,	"Parenthesis Error	"),
	("1+-+4",			errors["numvals"],	deg,	"Token Number Error	"),
	("2+1-",			errors["numvals"],	deg,	"Token Number Error	"),
]

def test_calc(program, test, expected, mode, description):
	command = '%s -m %s "%s"' % (program, mode, test)
	output = getstatusoutput(command)[1]
	if output.replace("\n", "") == expected:
		print "%s ... OK" % (description)
	else:
		print "%s ... FAIL" % (description)
		print 'Expression: "%s"' % (test)
		print 'Output: "%s"' % (output)
		print 'Expected: "%s"' % (expected)

def run_tests(program):
	failures, counter = [0 for x in 'a'*2]
	for case in CASES:
		counter += 1
		if test_calc(program, case[0], case[1], case[2], case[3]):
			failures += 1
	print "--- Test Summary ---"
	print "Cases run: %d" % counter
	if failures:
		print "Failed cases: %d" % failures
	else:
		print "All tests passed."

def main():
	print "--- Beginning Synge Test Suite ---"
	if len(argv) < 2:
		print "Error: no test executable given"
		return 1
	run_tests(argv[1])
	return 0

if __name__ == "__main__":
	exit(main())
