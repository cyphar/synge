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
		"zerodiv"	: "Cannot divide by zero",
		"zeromod"	: "Cannot modulo by zero",
		"lparen"	: "Missing closing bracket for opening bracket",
		"rparen"	: "Missing opening bracket for closing bracket",
		"token"		: "Unknown token or function in expression",
		"opvals"	: "Not enough values for operator",
		"funcvals"	: "Not enough arguments for function",
		"empty"		: "Expression was empty",
		"overflow"	: "Number caused overflow",
		"unknown"	: "An unknown error has occured"
}

def error_get(key, position = 0):
	if position:
		return ['%s @ %d' % (errors[key], position)]
	else:
		return [errors[key]]

# List of case tuples

CASES = [
#	[test expressions]			[expected results]	mode	test description

	# expected successes
	(["1+1"],				["2"],				0,	"Addition		"),
	(["41+1"],				["42"],				0,	"Addition		"),
	(["43.7+2.3"],				["46"],				0,	"Addition		"),
	(["1+-1"],				["0"],				0,	"Convoluted Addition	"),

	(["5-3"],				["2"],				0,	"Subtraction		"),
	(["-5-3"],				["-8"],				0,	"Subtraction		"),
	(["15-3.5-10"],				["1.5"],			0,	"Subtraction		"),

	(["3*4"],				["12"],				0,	"Multiplication		"),
	(["-2*6"],				["-12"],			0,	"Multiplication		"),

	(["4/2"],				["2"],				0,	"Division		"),
	(["-9/3"],				["-3"],				0,	"Division		"),
	(["5/0.2"],				["25"],				0,	"Division		"),
	(["-4.2/2.1"],				["-2"],				0,	"Division		"),

	(["4\\2"],				["2"],				0,	"Integer Division	"),
	(["-9\\3"],				["-3"],				0,	"Integer Division	"),
	(["5.2412\\1.223"],			["4"],				0,	"Integer Division	"),
	(["-4.2394\\2.3"],			["-1"],				0,	"Integer Division	"),

	(["5%2"],				["1"],				0,	"Modulo			"),
	(["13%-2"],				["1"],				0,	"Modulo			"),
	(["15.1%2"],				["1.1"],			0,	"Modulo			"),

	(["(1+1)*2"],				["4"],				0,	"Parenthesis		"),
	(["1+(-2)"],				["-1"],				0,	"Parenthesis		"),
	(["(0.5+0.5)*(1.5+1.5)"],		["3"],				0,	"Parenthesis		"),

	(["1(2)"],				["2"],				0,	"Implied Multiplication	"),
	(["-3(6)"],				["-18"],			0,	"implied multiplication	"),
	(["4(-3)"],				["-12"],			0,	"implied multiplication	"),
	(["-1(-3)"],				["3"],				0,	"implied multiplication	"),
	(["(2)8"],				["16"],				0,	"Implied Multiplication	"),
	(["-3(6)-2"],				["-20"],			0,	"implied multiplication	"),
	(["(-3)+2"],				["-1"],				0,	"implied multiplication	"),
	(["4(0.25)"],				["1"],				0,	"implied multiplication	"),
	(["-3.5(2)"],				["-7"],				0,	"Implied Multiplication	"),
	(["7.2(-5)"],				["-36"],			0,	"Implied Multiplication	"),
	(["-2.9(-2.4)"],			["6.96"],			0,	"Implied Multiplication	"),

	(["10^2"],				["100"],			0,	"Indicies		"),
	(["(-3)^2"],				["9"],				0,	"Indicies		"),
	(["16^(1/4)"],				["2"],				0,	"Fractional Indicies	"),
	(["16^(-1/4)"],				["0.5"],			0,	"Fractional Indicies	"),

	(["pi-pi%1"],				["3"],				0,	"Magic Numbers		"),
	(["e-e%1"],				["2"],				0,	"Magic Numbers		"),

	(["-0"],				["0"],				0,	"Negative Zero		"),

	(["0xDEADBEEF + 0xA"],			["3735928569"],			0,	"Hexadecimal Addition	"),
	(["0xDEADBEEF - 0xA"],			["3735928549"],			0,	"Hexadecimal Subtraction	"),
	(["0xA0 / 0xA"],			["16"],				0,	"Hexadecimal Division	"),

	(["0xDEADBEEF + 10"],			["3735928569"],			0,	"Mixed Addition		"),
	(["0xDEADBEEF - 10"],			["3735928549"],			0,	"Mixed Subtraction	"),
	(["0xA0 / 10"],				["16"],				0,	"Mixed Division		"),

	(["3.0+2.1"],				["5.1"],			0,	"Decimal Notation	"),
	(["5.3+2"],				["7.3"],			0,	"Decimal Notation	"),
	(["0.1+2"],				["2.1"],			0,	"Decimal Notation	"),
	(["1.0+3"],				["4"],				0,	"Decimal Notation	"),
	([".1+2"],				["2.1"],			0,	"Decimal Notation	"),
	(["1.+3"],				["4"],				0,	"Decimal Notation	"),

	(["abs(-123)"],				["123"],			0,	"Assorted Functions	"),
	(["abs(123)"],				["123"],			0,	"Assorted Functions	"),
	(["sqrt(4)"],				["2"],				0,	"Assorted Functions	"),
	(["cbrt(8)"],				["2"],				0,	"Assorted Functions	"),
	(["floor(0.5)"],			["0"],				0,	"Assorted Functions	"),
	(["round(0.5)"],			["1"],				0,	"Assorted Functions	"),
	(["ceil(0.5)"],				["1"],				0,	"Assorted Functions	"),
	(["log10(1000)"],			["3"],				0,	"Assorted Functions	"),
	(["log(1024)"],				["10"],				0,	"Assorted Functions	"),
	(["ln(e^2)"],				["2"],				0,	"Assorted Functions	"),
	(["fact(3)"],				["6"],				0,	"Assorted Functions	"),
	(["fact(4)"],				["24"],				0,	"Assorted Functions	"),

	(["log10(100)/2"],			["1"],				0,	"Function Division	"),
	(["ln(100)/ln(10)"],			["2"],				0,	"Function Division	"),
	(["ceil(11.01)/floor(12.01)"],		["1"],				0,	"Function Division	"),

	(["3+4*2/(1-5)^2^3"],			["3.0001220703"],		0,	"Operator Precedence	"),

	(["rand(100)"],				["32"],				0,	'"Random" Number		'),
	(["rand(26)"],				["19"],				0,	'"Random" Number		'),
	(["rand(146.7)"],			["64"],				0,	'"Random" Number		'),
	(["rand(32)"],				["28"],				0,	'"Random" Number		'),

	(["tan(45)+cos(60)+sin(30)"],		["2"],				deg,	"Degrees Trigonometry	"),
	(["atan(1)+acos(0.5)+asin(0)"],		["105"],			deg,	"Degrees Trigonometry	"),
	(["atan(sin(30)/cos(30))"],		["30"],				deg,	"Degrees Trigonometry	"),

	(["tan(45)+cos(60)+sin(30)"],		["-0.320669414"],		rad,	"Radian Trigonometry	"),
	(["atan(1)+acos(0.5)+asin(0)"],		["1.8325957146"],		rad,	"Radian Trigonometry	"),
	(["atan(sin(1.1)/cos(1.1))"],		["1.1"],			rad,	"Radian Trigonometry	"),

	(["tanh(ln(2))"],			["0.6"],			0,	"Hyperbolic Trigonometry	"),
	(["e^atanh(0.6)"],			["2"],				0,	"Hyperbolic Trigonometry	"),
	(["cosh(ln(2))"],			["1.25"],			0,	"Hyperbolic Trigonometry	"),
	(["e^acosh(1.25)"],			["2"],				0,	"Hyperbolic Trigonometry	"),
	(["sinh(ln(2))"],			["0.75"],			0,	"Hyperbolic Trigonometry	"),
	(["e^asinh(0.75)"],			["2"],				0,	"Hyperbolic Trigonometry	"),

	(["deg2rad(180/pi)+rad2deg(pi)"],	["181"],			0,	"Angle Conversion	"),

	(["2^2^2-15-14-12-11-10^5"],		["-100036"],			0,	"Long Expression		"),
	(["57-(-2)^2-floor(log10(23))"],	["52"],				0,	"Long Expression		"),
	(["2+floor(log10(23))/32"],		["2.03125"],			0,	"Long Expression		"),
	(["ceil((e%2.5)*300)"],			["66"],				0,	"Long Expression		"),
	(["(fact(4)+8)/4+(2+3*4)^2-3^4+(9+10)*(15-21)+7*(17-15)"],
						["23"],				0,	"Long Expression		"),

	(["987654321012/987654321012"],		["1"],				0,	"Big Numbers		"),

	(["1 +1 "],				["2"],				0,	"Spaces			"),
	(["  41 + 1"],				["42"],				0,	"Spaces			"),
	([" 43.7+ 2.3  "],			["46"],				0,	"Spaces			"),
	(["-3( 6)"],				["-18"],			0,	"Spaces			"),
	(["4 ( -3 )"],				["-12"],			0,	"Spaces			"),
	(["-1  (-3)"],				["3"],				0,	"Spaces			"),
	([" 4( 0.25)"],				["1"],				0,	"Spaces			"),
	(["  -3.5( 2)"],			["-7"],				0,	"Spaces			"),
	(["1+  -1"],				["0"],				0,	"Spaces			"),
	([" 5  -3"],				["2"],				0,	"Spaces			"),
	(["( -3  )   ^ 2"],			["9"],				0,	"Spaces			"),
	(["16  ^( 1 /  4 )"],			["2"],				0,	"Spaces			"),
	(["3 +4  *2 / ( 1- 5)^ 2 ^ 3"],		["3.0001220703"],		0,	"Spaces			"),

	(["a=2", "a", "a=3a+4", "a"],		["2", "2", "10", "10"],		0,	"Basic Variables		"),
	(["test=2", "test", "a=test^2-7", "a+2", "2test+1"],
					["2", "2", "-3", "-1", "5"],		0,	"Basic Variables		"),

	(["a=-2", "y:=a+3", "y+3", "a=2", "2y-1"],
						["-2", "1", "4", "2", "9"],	0,	"Basic User Functions	"),

	(["a=3", "y:=3+a", "a", "y", "y+a", "a+y"],
						["3", "6", "3", "6", "9", "9"],	0,	"Functions and Variables	"),

	# expected errors
	([""],					error_get("empty"),		0,	"Empty Expression Error	"),
	([" "],					error_get("empty"),		0,	"Empty Expression Error	"),

	(["does_not_exist"],			error_get("token", 1),		0,	"Unknown Token Error	"),
	(["fake_function()"],			error_get("token", 1),		0,	"Unknown Token Error	"),
	(["fake_function()+23"],		error_get("token", 1),		0,	"Unknown Token Error	"),
	(["1@5"],				error_get("token", 2),		0,	"Unknown Token Error	"),

	(["1/0"],				error_get("zerodiv", 2),	0,	"Zero Division Error	"),
	(["1\\0"],				error_get("zerodiv", 2),	0,	"Zero Division Error	"),

	(["1/cos(90)"],				error_get("zerodiv", 2),	deg,	"Zero Division Error	"),
	(["1\\sin(0)"],				error_get("zerodiv", 2),	deg,	"Zero Division Error	"),

	(["1%(2-(2^2/2))"],			error_get("zeromod", 2),	0,	"Modulo by Zero Error	"),
	(["1%(cos(45) - sin(45))"],		error_get("zeromod", 2),	deg,	"Modulo by Zero Error	"),

	(["1+(1"],				error_get("lparen", 3),		0,	"Parenthesis Error	"),
	(["1+((1"],				error_get("lparen", 4),		0,	"Parenthesis Error	"),

	(["1+4)"],				error_get("rparen", 4),		0,	"Parenthesis Error	"),
	(["1+(4))"],				error_get("rparen", 6),		0,	"Parenthesis Error	"),

	(["1+-+4"],				error_get("opvals", 2),		0,	"Token Number Error	"),
	(["2+1-"],				error_get("opvals", 4),		0,	"Token Number Error	"),
	(["abs()"],				error_get("funcvals", 1),	0,	"Token Number Error	"),
	(["100000000000000000000000000"],	error_get("overflow", 1),	0,	"Input Overflow Error	"),
	(["231664726992975794912959502"],	error_get("overflow", 1),	0,	"Input Overflow Error	"),
	(["1+1000000000000000000000000"],	error_get("overflow", 3),	0,	"Input Overflow Error	"),
	(["17^68"],				error_get("overflow", 3),	0,	"Output Overflow Error	"),
	(["100^23"],				error_get("overflow", 4),	0,	"Output Overflow Error	"),
]

def test_calc(program, test, expected, mode, description):
	command = '%s -m "%s" %s' % (program, mode, '"' + '" "'.join(test) + '"')
	output = getstatusoutput(command)[1]
	if output == '\n'.join(expected):
		print "%s ... %sOK%s" % (description, ansi_good, ansi_reset)
		return True
	else:
		print "%s ... %sFAIL%s" % (description, ansi_error, ansi_reset)

		if len(expected) > 1:
			print "\tExpressions:"
			for each in test:
				print '\t\t- "%s%s%s"' % (ansi_good, each, ansi_reset)
		else:
			print '\tExpression: "%s%s%s"' % (ansi_good, test[0], ansi_reset)

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
		print "%sFailed cases: %d%s" % (ansi_error, failures, ansi_reset)
	else:
		print "%sAll tests passed%s" % (ansi_good, ansi_reset)

def main():
	print "--- Beginning Synge Test Suite ---"
	if len(argv) < 2:
		print "Error: no test executable given"
		return 1
	run_tests(argv[1])
	return 0

if __name__ == "__main__":
	exit(main())
