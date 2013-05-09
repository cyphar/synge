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
from commands import getstatusoutput


deg = "degrees"
rad = "radians"

ansi_error = "\x1b[1;31m"
ansi_good  = "\x1b[1;32m"
ansi_reset = "\x1b[0m"

errors = {
		"zerodiv"	: ["Attempted to divide or modulo by zero."],
		"parens"	: ["Missing parenthesis in expression."],
		"token"		: ["Unknown token or function in expression."],
		"numvals"	: ["Incorrect number of values for operator or function."],
		"empty"		: ["Expression was empty."],
		"overflow"	: ["Number caused overflow."],
		"unknown"	: ["An unknown error has occured."]
}

# List of case tuples

CASES = [
#	test expression			[expected results]	mode	test description

	# expected successes
	("1+1",				["2"],				0,	"Addition		"),
	("1+-1",			["0"],				0,	"Convoluted Addition	"),
	("5-3",				["2"],				0,	"Subtraction		"),
	("0-5-3",			["-8"],				0,	"Subtraction		"),
	("3*4",				["12"],				0,	"Multiplication		"),
	("4/2",				["2"],				0,	"Division		"),
	("5%2",				["1"],				0,	"Modulo			"),
	("15.1%2",			["1.0999999999999996"],		0,	"Modulo			"), # needs to be fixed (due to rounding err)
	("(1+1)*2",			["4"],				0,	"Parenthesis		"),
	("1+(-2)",			["-1"],				0,	"Parenthesis		"),
	("10^2",			["100"],			0,	"Indicies		"),
	("16^(1/4)",			["2"],				0,	"Fractional Indicies	"),

	("pi-pi%1",			["3"],				0,	"Magic Numbers		"),
	("e-e%1",			["2"],				0,	"Magic Numbers		"),

	("0xDEADBEEF + 0xA",		["3735928569"],			0,	"Hexadecimal Addition	"),
	("0xDEADBEEF - 0xA",		["3735928549"],			0,	"Hexadecimal Subtraction	"),
	("0xA0 / 0xA",			["16"],				0,	"Hexadecimal Division	"),

	("0xDEADBEEF + 10",		["3735928569"],			0,	"Mixed Addition		"),
	("0xDEADBEEF - 10",		["3735928549"],			0,	"Mixed Subtraction	"),
	("0xA0 / 10",			["16"],				0,	"Mixed Division		"),

	("3.0+2.1",			["5.0999999999999996"],		0,	"Decimal Notation	"), # needs to be fixed (due to rounding err)
	("5.3+2",			["7.2999999999999998"],		0,	"Decimal Notation	"), # needs to be fixed (due to rounding err)
	("0.1+2",			["2.1"],			0,	"Decimal Notation	"),
	("1.0+3",			["4"],				0,	"Decimal Notation	"),
	(".1+2",			["2.1"],			0,	"Decimal Notation	"),
	("1.+3",			["4"],				0,	"Decimal Notation	"),

	("log10(100)/2",		["1"],				0,	"Function Division	"),
	("ln(100)/ln(10)",		["2"],				0,	"Function Division	"),
	("ceil(11.01)/floor(12.01)",	["1"],				0,	"Function Division	"),

	("tan(45)+cos(60)+sin(30)",	["2", "1.9999999999999998"],	deg,	"Degrees Trigonometry	"),
	("atan(1)+acos(0.5)+asin(0)",	["105"],			deg,	"Degrees Trigonometry	"),
	("atan(sin(30)/cos(30))",	["29.9999999999999964",
					 "30",
					 "30.0000000000000036"],	deg,	"Degrees Trigonometry	"),

	("tan(45)+cos(60)+sin(30)",	["-0.320669413964157"],		rad,	"Radian Trigonometry	"),
	("atan(1)+acos(0.5)+asin(0)",	["1.832595714594046"],		rad,	"Radian Trigonometry	"),
	("atan(sin(1.1)/cos(1.1))",	["1.1"],			rad,	"Radian Trigonometry	"),

	("tanh(ln(2))",			["0.6"],			0,	"Hyperbolic Trigonometry	"),
	("e^atanh(0.6)",		["1.9999999999999998", "2"],	0,	"Hyperbolic Trigonometry	"), # 'nother rounding error
	("cosh(ln(2))",			["1.25"],			0,	"Hyperbolic Trigonometry	"),
	("e^acosh(1.25)",		["1.9999999999999998", "2"],	0,	"Hyperbolic Trigonometry	"), # 'nother rounding error
	("sinh(ln(2))",			["0.75"],			0,	"Hyperbolic Trigonometry	"),
	("e^asinh(0.75)",		["1.9999999999999998", "2"],	0,	"Hyperbolic Trigonometry	"), # 'nother rounding error

	("deg2rad(180/pi)+rad2deg(pi)",	["181"],			0,	"Angle Conversion	"),

	("2^2^2-15-14-12-11-10^5",	["-100036"],			0,	"Complex Expression	"),
	("57-(-2)^2-floor(log10(23))",	["52"],				0,	"Complex Expression	"),
	("2+floor(log10(23))/32",	["2.03125"],			0,	"Complex Expression	"),
	("ceil((e%2.5)*300)",		["66"],				0,	"Complex Expression	"),

	("987654321012/987654321012",	["1"],				0,	"Big Numbers		"),

	# expected errors
	("",				errors["empty"],		0,	"Empty Expression Error	"),
	(" ",				errors["empty"],		0,	"Empty Expression Error	"),
	("does_not_exist",		errors["token"],		0,	"Unknown Token Error	"),
	("fake_function()",		errors["token"],		0,	"Unknown Token Error	"),
	("fake_function()+23",		errors["token"],		0,	"Unknown Token Error	"),
	("1@5",				errors["token"],		0,	"Unknown Token Error	"),
	("1/0",				errors["zerodiv"],		0,	"Zero Division Error	"),
	("1%(2-(2^2/2))",		errors["zerodiv"],		0,	"Modulo by Zero Error	"),
	("1+(1",			errors["parens"],		0,	"Parenthesis Error	"),
	("1+4)",			errors["parens"],		0,	"Parenthesis Error	"),
	("1+-+4",			errors["numvals"],		0,	"Token Number Error	"),
	("2+1-",			errors["numvals"],		0,	"Token Number Error	"),
	("abs()",			errors["numvals"],		0,	"Token Number Error	"),
	("100000000000000000000000000", errors["overflow"],		0,	"Input Overflow Error	"),
	("17^68",			errors["overflow"],		0,	"Output Overflow Error	"),
]

def test_calc(program, test, expected, mode, description):
	command = '%s -m "%s" "%s"' % (program, mode, test)
	output = getstatusoutput(command)[1]
	if output.replace("\n", "") in expected:
		print "%s ... %sOK%s" % (description, ansi_good, ansi_reset)
		return True
	else:
		print "%s ... %sFAIL%s" % (description, ansi_error, ansi_reset)
		print '\tExpression: "%s"' % (test)
		print '\tOutput: "%s%s%s"' % (ansi_error, output, ansi_reset)
		if len(expected) > 1:
			print "\tExpected:"
			for each in expected:
				print '\t\t- "%s%s%s"' % (ansi_good, each, ansi_reset)
		else:
			print '\tExpected: "%s%s%s"' % (ansi_good, expected[0], ansi_reset)
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
