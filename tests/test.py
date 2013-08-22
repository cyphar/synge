#!/usr/bin/python -tt

# Synge: A shunting-yard calculation "engine"
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

from os import popen, name
from sys import argv

deg = "degrees"
rad = "radians"

if name is not "nt":
	ansi_error = "\x1b[1;31m"
	ansi_warn  = "\x1b[1;33m"
	ansi_good  = "\x1b[1;32m"
	ansi_reset = "\x1b[0m"
else:
	ansi_error = ansi_warn = ansi_good = ansi_reset = ""

errors = {
		"zerodiv"	: "Cannot divide by zero",
		"zeromod"	: "Cannot modulo by zero",
		"lparen"	: "Missing closing bracket for opening bracket",
		"rparen"	: "Missing opening bracket for closing bracket",
		"token"		: "Unknown token or function in expression",
		"assign"	: "Invalid left operand of assignment",
		"word"		: "Invalid word to delete",
		"delword"	: "Unknown word to delete",
		"funcvals"	: "Not enough arguments for function",
		"opvals"	: "Not enough values for operator",
		"ifop"		: "Missing if operator for else",
		"elseop"	: "Missing else operator for if",
		"ifblock"	: "Missing if block for else",
		"elseblock"	: "Missing else block for if",
		"toomany"	: "Too many values in expression",
		"empty"		: "Expression was empty",
		"undef"		: "Result is undefined",
		"delved"	: "Delved too deep",
		"unknown"	: "An unknown error has occured",
}

def error_get(key, position = 0):
	if position:
		return '%s @ %d' % (errors[key], position)
	else:
		return errors[key]

# List of case tuples

CASES = [
#	[test expressions]			[expected results]		mode	changes		test description

	# expected successes
	(["+42"],				["42"],				0,	0,		"Number			"),
	(["-42"],				["-42"],			0,	0,		"Number			"),

	(["1+1"],				["2"],				0,	0,		"Addition		"),
	(["41+1"],				["42"],				0,	0,		"Addition		"),
	(["43.7+2.3"],				["46"],				0,	0,		"Addition		"),
	(["1+-1"],				["0"],				0,	0,		"Convoluted Addition	"),
	(["1+-+4"],				["-3"],				0,	0,		"Convoluted Addition	"),

	(["5-3"],				["2"],				0,	0,		"Subtraction		"),
	(["-5-3"],				["-8"],				0,	0,		"Subtraction		"),
	(["15-3.5-10"],				["1.5"],			0,	0,		"Subtraction		"),
	(["1-+1"],				["0"],				0,	0,		"Convoluted Subtraction	"),

	(["3*4"],				["12"],				0,	0,		"Multiplication		"),
	(["-2*6"],				["-12"],			0,	0,		"Multiplication		"),

	(["4/2"],				["2"],				0,	0,		"Division		"),
	(["-9/3"],				["-3"],				0,	0,		"Division		"),
	(["5/0.2"],				["25"],				0,	0,		"Division		"),
	(["-4.2/2.1"],				["-2"],				0,	0,		"Division		"),

	(["4//2"],				["2"],				0,	0,		"Integer Division	"),
	(["-9//3"],				["-3"],				0,	0,		"Integer Division	"),
	(["5.2412//1.223"],			["4"],				0,	0,		"Integer Division	"),
	(["-4.2394//2.3"],			["-1"],				0,	0,		"Integer Division	"),

	(["5%2"],				["1"],				0,	0,		"Modulo			"),
	(["13%-2"],				["1"],				0,	0,		"Modulo			"),
	(["15.1%2"],				["1.1"],			0,	0,		"Modulo			"),

	(["5|2"],				["7"],				0,	0,		"Bitwise OR		"),
	(["8|-2"],				["-2"],				0,	0,		"Bitwise OR		"),
	(["15.1|2"],				["15"],				0,	0,		"Bitwise OR		"),

	(["5&2"],				["0"],				0,	0,		"Bitwise AND		"),
	(["7&-2"],				["6"],				0,	0,		"Bitwise AND		"),
	(["15.1&5"],				["5"],				0,	0,		"Bitwise AND		"),

	(["5#2"],				["7"],				0,	0,		"Bitwise XOR		"),
	(["8#-2"],				["-10"],			0,	0,		"Bitwise XOR		"),
	(["15.1#2"],				["13"],				0,	0,		"Bitwise XOR		"),

	(["~5"],				["-6"],				0,	0,		"Bitwise NOT		"),
	(["~-2"],				["1"],				0,	0,		"Bitwise NOT		"),
	(["~15.1"],				["-16"],			0,	0,		"Bitwise NOT		"),
	(["~15.9"],				["-17"],			0,	0,		"Bitwise NOT		"),

	(["~5"],				["-6"],				0,	0,		"Bitwise NOT Sign	"),
	(["-~2"],				["3"],				0,	0,		"Bitwise NOT Sign	"),
	(["~12.3"],				["-13"],			0,	0,		"Bitwise NOT Sign	"),
	(["-~15.3"],				["16"],				0,	0,		"Bitwise NOT Sign	"),
	(["~(-3)"],				["2"],				0,	0,		"Bitwise NOT Sign	"),
	(["~(3)"],				["-4"],				0,	0,		"Bitwise NOT Sign	"),

	(["!2"],				["0"],				0,	0,		"Unary NOT		"),
	(["!34"],				["0"],				0,	0,		"Unary NOT		"),
	(["!0"],				["1"],				0,	0,		"Unary NOT		"),
	(["!0.1"],				["0"],				0,	0,		"Unary NOT		"),
	(["!(56)"],				["0"],				0,	0,		"Unary NOT Sign		"),
	(["!(1-1)"],				["1"],				0,	0,		"Unary NOT Sign		"),
	(["-!(3-3)"],				["-1"],				0,	0,		"Unary NOT Sign		"),

	(["(1+1)*2"],				["4"],				0,	0,		"Parenthesis		"),
	(["1+(-2)"],				["-1"],				0,	0,		"Parenthesis		"),
	(["(0.5+0.5)*(1.5+1.5)"],		["3"],				0,	0,		"Parenthesis		"),

	(["1(2)"],				["2"],				0,	0,		"Implied Multiplication	"),
	(["-3(6)"],				["-18"],			0,	0,		"implied Multiplication	"),
	(["4(-3)"],				["-12"],			0,	0,		"implied Multiplication	"),
	(["-1(-3)"],				["3"],				0,	0,		"implied Multiplication	"),
	(["(2)8"],				["16"],				0,	0,		"Implied Multiplication	"),
	(["-3(6)-2"],				["-20"],			0,	0,		"implied Multiplication	"),
	(["(-3)+2"],				["-1"],				0,	0,		"implied Multiplication	"),
	(["4(0.25)"],				["1"],				0,	0,		"implied Multiplication	"),
	(["-3.5(2)"],				["-7"],				0,	0,		"Implied Multiplication	"),
	(["7.2(-5)"],				["-36"],			0,	0,		"Implied Multiplication	"),
	(["-2.9(-2.4)"],			["6.96"],			0,	0,		"Implied Multiplication	"),

	(["2tan(45)"],				["2"],				deg,	0,		"Implied Multiplication	"),
	(["-3cos(0)"],				["-3"],				deg,	0,		"Implied Multiplication	"),
	(["0.3sin(90)"],			["0.3"],			deg,	0,		"Implied Multiplication	"),
	(["-2.1acos(0.5)"],			["-126"],			deg,	0,		"Implied Multiplication	"),

	(["10^2"],				["100"],			0,	0,		"Indicies		"),
	(["(-3)^2"],				["9"],				0,	0,		"Indicies		"),
	(["16^(1/4)"],				["2"],				0,	0,		"Fractional Indicies	"),
	(["16^(-1/4)"],				["0.5"],			0,	0,		"Fractional Indicies	"),

	(["pi-pi%1"],				["3"],				0,	0,		"Magic Numbers		"),
	(["e-e%1"],				["2"],				0,	0,		"Magic Numbers		"),

	(["-0"],				["0"],				0,	0,		"Negative Zero		"),

	(["0xA0"],				["160"],			0,	0,		"Hexadecimal		"),
	(["+0xA0"],				["160"],			0,	0,		"Hexadecimal		"),
	(["-0xA0"],				["-160"],			0,	0,		"Hexadecimal		"),

	(["0xDEADBEEF + 0xA"],			["3735928569"],			0,	0,		"Hex Addition		"),
	(["0xDEADBEEF - 0xA"],			["3735928549"],			0,	0,		"Hex Subtraction		"),
	(["0xA0 * 0xA"],			["1600"],			0,	0,		"Hex Multiplication	"),
	(["0xA0 / 0xA"],			["16"],				0,	0,		"Hex Division		"),

	(["0b11"],				["3"],				0,	0,		"Binary			"),
	(["+0b11"],				["3"],				0,	0,		"Binary			"),
	(["-0b11"],				["-3"],				0,	0,		"Binary			"),

	(["0b10 + 0b01"],			["3"],				0,	0,		"Binary Addition		"),
	(["0b10 - 0b01"],			["1"],				0,	0,		"Binary Subtraction	"),
	(["0b100 * 0b10"],			["8"],				0,	0,		"Binary Multiplication	"),
	(["0b01 / 0b10"],			["0.5"],			0,	0,		"Binary Division		"),

	(["010"],				["8"],				0,	0,		"Octal			"),
	(["+010"],				["8"],				0,	0,		"Octal			"),
	(["-010"],				["-8"],				0,	0,		"Octal			"),

	(["010 + 01"],				["9"],				0,	0,		"Octal Addition		"),
	(["0010 - 0o1"],			["7"],				0,	0,		"Octal Subtraction	"),
	(["0100 * 0010"],			["512"],			0,	0,		"Octal Multiplication	"),
	(["0o100 / 0o10"],			["8"],				0,	0,		"Octal Division		"),

	(["0xDEADBEEF + 10"],			["3735928569"],			0,	0,		"Mixed Addition		"),
	(["0b10 - 10"],				["-8"],				0,	0,		"Mixed Subtraction	"),
	(["010 / 0d2"],				["4"],				0,	0,		"Mixed Division		"),

	(["3.0+2.1"],				["5.1"],			0,	0,		"Decimal Notation	"),
	(["5.3+2"],				["7.3"],			0,	0,		"Decimal Notation	"),
	(["0.1+2"],				["2.1"],			0,	0,		"Decimal Notation	"),
	(["1.0+3"],				["4"],				0,	0,		"Decimal Notation	"),
	([".1+2"],				["2.1"],			0,	0,		"Decimal Notation	"),
	(["1.+3"],				["4"],				0,	0,		"Decimal Notation	"),

	(["abs(-123)"],				["123"],			0,	0,		"Assorted Functions	"),
	(["abs(123)"],				["123"],			0,	0,		"Assorted Functions	"),
	(["sqrt(4)"],				["2"],				0,	0,		"Assorted Functions	"),
	(["cbrt(8)"],				["2"],				0,	0,		"Assorted Functions	"),
	(["floor(0.5)"],			["0"],				0,	0,		"Assorted Functions	"),
	(["round(0.5)"],			["1"],				0,	0,		"Assorted Functions	"),
	(["ceil(0.5)"],				["1"],				0,	0,		"Assorted Functions	"),
	(["log10(1000)"],			["3"],				0,	0,		"Assorted Functions	"),
	(["log(1024)"],				["10"],				0,	0,		"Assorted Functions	"),
	(["ln(e^2)"],				["2"],				0,	0,		"Assorted Functions	"),
	(["fact(3)"],				["6"],				0,	0,		"Assorted Functions	"),
	(["fact(4)"],				["24"],				0,	0,		"Assorted Functions	"),

	(["randi(100)"],			["52"],				0,	0,		"'Random' Function	"),
	(["randi(13)"],				["7"],				0,	0,		"'Random' Function	"),
	(["randi(14.7)"],			["7"],				0,	0,		"'Random' Function	"),
	(["randi(42)"],				["22"],				0,	0,		"'Random' Function	"),
	(["randi(52)"],				["27"],				0,	0,		"'Random' Function	"),

	(["log10(100)/2"],			["1"],				0,	0,		"Function Division	"),
	(["ln(100)/ln(10)"],			["2"],				0,	0,		"Function Division	"),
	(["ceil(11.01)/floor(12.01)"],		["1"],				0,	0,		"Function Division	"),

	(["3+4*2/(1-5)^2^3"],			["3.0001220703125"],		0,	0,		"Operator Precedence	"),

	(["(1+3)?2-5:23"],			["-3"],				0,	0,		"Conditional Statement	"),
	(["(-3<0)?43.3*(3-2):3.5"],		["43.3"],			0,	0,		"Conditional Statement	"),
	(["0?1.5-3/2:16-3.5"],			["12.5"],			0,	0,		"Conditional Statement	"),
	(["(5!=0)?5.2:5.5"],			["5.2"],			0,	0,		"Conditional Statement	"),
	(["0.001?19.2:4.2"],			["19.2"],			0,	0,		"Conditional Statement	"),
	(["true?1:1/0"],			["1"],				0,	0,		"Conditional Statement	"),
	(["false?1/0:1"],			["1"],				0,	0,		"Conditional Statement	"),
	(["(true?a=3:a=0)+false?a=4:a=2", "a"],
	 ["5",                            "2"],					0,	0,		"Conditional Statement	"),
	(["true?a=3:a=0", "a"],
	 ["3",            "3"],							0,	0,		"Conditional Statement	"),
	(["false?a=2:a=12", "a"],
	 ["12",             "12"],						0,	0,		"Conditional Statement	"),
	(["a=1+false?a=2:a=12", "a"],
	 ["13",                 "13"],						0,	0,		"Conditional Statement	"),

	(["tan(45)+cos(60)+sin(30)"],		["2"],				deg,	0,		"Degrees Trigonometry	"),
	(["atan(1)+acos(0.5)+asin(0)"],		["105"],			deg,	0,		"Degrees Trigonometry	"),
	(["atan(sin(30)/cos(30))"],		["30"],				deg,	0,		"Degrees Trigonometry	"),

	(["tan(45)+cos(60)+sin(30)"],
	["-0.3206694139641565326987689858924589712956105145341298199928108166"],rad,	0,		"Radian Trigonometry	"),
	(["atan(1)+acos(0.5)+asin(0)"],
	["1.8325957145940460557698753069130433491150154829688117289020510122"],	rad,	0,		"Radian Trigonometry	"),
	(["atan(sin(1.1)/cos(1.1))"],		["1.1"],			rad,	0,		"Radian Trigonometry	"),

	(["tanh(ln(2))"],			["0.6"],			0,	0,		"Hyperbolic Trigonometry	"),
	(["e^atanh(0.6)"],			["2"],				0,	0,		"Hyperbolic Trigonometry	"),
	(["cosh(ln(2))"],			["1.25"],			0,	0,		"Hyperbolic Trigonometry	"),
	(["e^acosh(1.25)"],			["2"],				0,	0,		"Hyperbolic Trigonometry	"),
	(["sinh(ln(2))"],			["0.75"],			0,	0,		"Hyperbolic Trigonometry	"),
	(["e^asinh(0.75)"],			["2"],				0,	0,		"Hyperbolic Trigonometry	"),

	(["deg2rad(180/pi)+rad2deg(pi)"],	["181"],			0,	0,		"Angle Conversion	"),

	(["2^2^2-15-14-12-11-10^5"],		["-100036"],			0,	0,		"Long Expression		"),
	(["57-(-2)^2-floor(log10(23))"],	["52"],				0,	0,		"Long Expression		"),
	(["2+floor(log10(23))/32"],		["2.03125"],			0,	0,		"Long Expression		"),
	(["ceil((e%2.5)*300)"],			["66"],				0,	0,		"Long Expression		"),
	(["(fact(4)+8)/4+(2+3*4)^2-3^4+(9+10)*(15-21)+7*(17-15)"],
						["23"],				0,	0,		"Long Expression		"),

	(["987654321012/987654321012"],		["1"],				0,	0,		"Big Numbers		"),

	(["1 +1 "],				["2"],				0,	0,		"Spaces			"),
	(["  41 + 1"],				["42"],				0,	0,		"Spaces			"),
	([" 43.7+ 2.3  "],			["46"],				0,	0,		"Spaces			"),
	(["-3( 6)"],				["-18"],			0,	0,		"Spaces			"),
	(["4 ( -3 )"],				["-12"],			0,	0,		"Spaces			"),
	(["-1  (-3)"],				["3"],				0,	0,		"Spaces			"),
	([" 4( 0.25)"],				["1"],				0,	0,		"Spaces			"),
	(["  -3.5( 2)"],			["-7"],				0,	0,		"Spaces			"),
	(["1+  -1"],				["0"],				0,	0,		"Spaces			"),
	([" 5  -3"],				["2"],				0,	0,		"Spaces			"),
	(["( -3  )   ^ 2"],			["9"],				0,	0,		"Spaces			"),
	(["16  ^( 1 /  4 )"],			["2"],				0,	0,		"Spaces			"),
	(["3 +4  *2 / ( 1- 5)^ 2 ^ 3"],		["3.0001220703125"],		0,	0,		"Spaces			"),

	(["3 ? +4 : 2"],			["4"],				0,	0,		"Spaces			"),
	(["3 ? +4: 2"],				["4"],				0,	0,		"Spaces			"),
	(["3? +4 : 2"],				["4"],				0,	0,		"Spaces			"),
	(["3? +4:2"],				["4"],				0,	0,		"Spaces			"),
	(["3? +4  :2 "],			["4"],				0,	0,		"Spaces			"),

	(["a=2", "a", "a=3a+4", "a"],		["2", "2", "10", "10"],		0,	0,		"Basic Variables		"),

	(["test=2", "test", "a=test^2-7", "a+2", "2test+1"],
	 ["2",      "2",    "-3",         "-1",  "5"],				0,	0,		"Basic Variables		"),

	(["a=b=3", "2b+a", "b=c=5-a", "a+b-c"],
	 ["3",     "9",    "2",       "3"],					0,	0,		"Variable Chaining	"),

	(["c=(a=2(b=3))-1", "a", "2b", "0.2c", "c=(a=3)(b=4)+2", "a+2b"],
	 ["5",              "6", "6",  "1",    "14",             "11"],		0,	0,		"Variable Returns	"),

	(["3(x=3)", "x", "3(x=2)+1/0",	          "x"],
	 ["9",      "3", error_get("zerodiv", 9), "3"],				0,	0,		"Variable Rollback	"),

	(["a=-2", "y:=a+3", "y+3", "a=2", "2y-1"],
	 ["-2",   "1",      "4",   "2",   "9"],					0,	0,		"Basic Functions		"),

	(["a=b=1", "f:=(a<b)?(a=a+b):(b=a+b)", "f", "f", "f", "f"],
	 ["1",	   "2",			       "3", "5", "8", "13"],		0,	0,		"Recursive Functions	"),

	(["x=5", "f:=(x>0)?x--+f:0", "x=10", "f",  "x=32", "f",   "x"],
	 ["5",	 "15",		     "10",   "55", "32",   "528", "0"],		0,	0,		"Recursive Functions	"),

	(["x=5", "f:=(x>0)?x--*f:1", "x=10", "f",	"x"],
	 ["5",	 "120",		     "10",   "3628800", "0"],			0,	0,		"Recursive Functions	"),

	(["x=2", "c:=(a:=2(b:=x+1))-1", "a+2b-0.2c", "x=1", "2a+3b-c"],
	 ["2",   "5",                   "11",        "1",   "11"],		0,	0,		"Function Returns	"),

	(["a=1.5", "3(x:=2a)", "x", "3(x:=a)+1/0",	      "x", "a=1", "x"],
	 ["1.5",   "9",        "3", error_get("zerodiv", 10), "3", "1",   "2"],	0,	0,		"Function Rollback	"),

	(["a=3", "y:=3+a", "a", "y", "y+a", "a+y"],
	 ["3",   "6",      "3", "6", "9",   "9"],				0,	0,		"Functions and Variables	"),

	(["a=3", "x=y:=3+a", "y+2x", "a=0", "2y+x"],
	 ["3",   "6",        "18",   "0",   "12"],				0,	0,		"Mixed Chaining		"),

	(["a=3", "::a", "a"],
	 ["3",   "3",   error_get("token", 1)],					0,	0,		"Variable Deletion	"),

	(["a=2", "y:=1/a", "a=5", "::y", "y"],
	 ["2",   "0.5",    "5",   "0.2", error_get("token", 1)],		0,	0,		"Function Deletion	"),

	(["a=2", "a++", "a", "++a"],		["2", "2", "3", "4"],		0,	0,		"Increment Variable	"),
	(["a=3", "a--", "a", "--a"],		["3", "3", "2", "1"],		0,	0,		"Decrement Variable	"),

	(["a=5", " a +=2", "a", "a+=0.5", "a"],	["5", "7", "7", "7.5", "7.5"],	0,	0,		"Compound Assignment	"),
	(["a=1", " a-= 2", "a", "a-=0.5", "a"],	["1","-1","-1","-1.5","-1.5"],	0,	0,		"Compound Assignment	"),
	(["a=3", "a *=2", "a", "a*=0.5", "a"],	["3", "6", "6", "3", "3"],	0,	0,		"Compound Assignment	"),
	(["a=3", "a /=2", "a", "a/=0.5", "a"],	["3", "1.5", "1.5", "3", "3"],	0,	0,		"Compound Assignment	"),
	(["a=3", "a//=2", "a", "a//=0.5", "a"],	["3", "1", "1", "2", "2"],	0,	0,		"Compound Assignment	"),
	([" a=8", "a%=5", "a", "a%=3", "a"],	["8", "3", "3", "0", "0"],	0,	0,		"Compound Assignment	"),
	(["a =3", "a ^=2", "a", "a^=0.5", "a"],	["3", "9", "9", "3", "3"],	0,	0,		"Compound Assignment	"),
	(["a=3", " a#= 2", "a", "a#=2", "a"],	["3", "1", "1", "3", "3"],	0,	0,		"Compound Assignment	"),
	(["a=3", "a|=2", "a", "a|=8", "a"],	["3", "3", "3", "11", "11"],	0,	0,		"Compound Assignment	"),
	(["a=3", "a&=2", "a", "a&=1", "a"],	["3", "2", "2", "0", "0"],	0,	0,		"Compound Assignment	"),

	([" a =3", "-a", "+a", "(-a)", "(+a)", "4+a", "-a+4"],
	 ["3",   "-3", "3",  "-3",   "3",    "7",   "1"],			0,	0,		"Variable Signing	"),

	(["a =8", "-a", "+a", "(-a)", "(+a)", "4-a", "+a+4"],
	 ["8",   "-8", "8",  "-8",   "8",    "-4",  "12"],			0,	0,		"Variable Signing	"),

	(["life", "-life", "+life", "(-life)", "(+life)"],
	 ["42",   "-42",   "42",    "-42",     "42"],				0,	0,		"Constant Signing	"),

	(["life", "8-life", "8+life", "-life+8", "+life+8"],
	 ["42",   "-34",    "50",     "-34",     "50"],				0,	0,		"Constant Signing	"),

	# expected errors
	([""],					[error_get("empty")],		0,	0,		"Empty Expression Error	"),
	([" "],					[error_get("empty")],		0,	0,		"Empty Expression Error	"),

	(["0s45"],				[error_get("token", 2)],	0,	0,		"Unknown Base Error	"),
	(["0jk45"],				[error_get("token", 2)],	0,	0,		"Unknown Base Error	"),
	(["0_45"],				[error_get("token", 2)],	0,	0,		"Unknown Base Error	"),

	(["does_not_exist"],			[error_get("token", 1)],	0,	0,		"Unknown Token Error	"),
	(["fake_function()"],			[error_get("token", 1)],	0,	0,		"Unknown Token Error	"),
	(["fake_function()+23"],		[error_get("token", 1)],	0,	0,		"Unknown Token Error	"),
	(["1_5"],				[error_get("token", 2)],	0,	0,		"Unknown Token Error	"),

	(["1/0"],				[error_get("zerodiv", 2)],	0,	0,		"Zero Division Error	"),
	(["1//0"],				[error_get("zerodiv", 2)],	0,	0,		"Zero Division Error	"),

	(["1/cos(90)"],				[error_get("zerodiv", 2)],	deg,	0,		"Zero Division Error	"),
	(["1//sin(0)"],				[error_get("zerodiv", 2)],	deg,	0,		"Zero Division Error	"),

	(["1%(2-(2^2/2))"],			[error_get("zeromod", 2)],	0,	0,		"Modulo by Zero Error	"),
	(["1%(cos(45) - sin(45))"],		[error_get("zeromod", 2)],	deg,	0,		"Modulo by Zero Error	"),

	(["1+(1"],				[error_get("lparen", 3)],	0,	0,		"Parenthesis Error	"),
	(["1+((1"],				[error_get("lparen", 4)],	0,	0,		"Parenthesis Error	"),

	(["1+4)"],				[error_get("rparen", 4)],	0,	0,		"Parenthesis Error	"),
	(["1+(4))"],				[error_get("rparen", 6)],	0,	0,		"Parenthesis Error	"),

	(["2+1-"],				[error_get("opvals", 4)],	0,	0,		"Token Number Error	"),
	(["abs()"],				[error_get("funcvals", 1)],	0,	0,		"Token Number Error	"),

	(["3?3"],				[error_get("elseop", 2)],	0,	0,		"Conditional Error	"),
	(["3?:3"],				[error_get("ifblock", 2)],	0,	0,		"Conditional Error	"),
	(["3?3:"],				[error_get("elseblock", 4)],	0,	0,		"Conditional Error	"),

	(["2--"],				[error_get("assign", 2)],	0,	0,		"Assign Error		"),
	(["--2"],				[error_get("assign", 1)],	0,	0,		"Assign Error		"),

	(["f:=f", "f"],				[error_get("unknown", 2), error_get("delved", 1)],
										0,	0,		"Recursion Error		"),

	(["a:=b", "b:=c", "c:=a", "1+a"], 	[error_get("unknown", 2), error_get("unknown", 2), error_get("unknown", 2), error_get("delved", 3)],
										0,	0,		"Recursion Error		"),
]

def test_calc(program, test, expected, mode, change, description):
	command = '%s -m "%s" %s' % (program, mode, '"' + '" "'.join(test) + '"')

	pipe = popen(command, 'r')
	output = pipe.readlines()

	output = ''.join(output).split('\n')

	if not output[-1]:
		output = output[:-1]

	if output == expected:
		print "%s ... %sOK%s" % (description, ansi_good, ansi_reset)
		return True
	elif change:
		print "%s ... %sWARN%s" % (description, ansi_warn, ansi_reset)
		return True
	else:
		print "%s ... %sFAIL%s" % (description, ansi_error, ansi_reset)

		if len(expected) > 1:
			print "\tExpressions:"
			for each in test:
				print '\t\t- "%s%s%s"' % (ansi_good, each, ansi_reset)
		else:
			print '\tExpression: "%s%s%s"' % (ansi_good, test[0], ansi_reset)

		if len(output) > 1:
			print "\tOutput:"
			for each in output:
				print '\t\t- "%s%s%s"' % (ansi_good, each, ansi_reset)
		else:
			print '\tOutput: "%s%s%s"' % (ansi_error, output[0], ansi_reset)

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
		if not test_calc(program, case[0], case[1], case[2], case[3], case[4]):
			failures += 1
	print "--- Test Summary ---"
	print "Cases run: %d" % (counter)
	if failures > 0:
		print "%sFailed cases: %d%s" % (ansi_error, failures, ansi_reset)
		return 1
	else:
		print "%sAll tests passed%s" % (ansi_good, ansi_reset)
		return 0

def main():
	print "--- Beginning Synge Test Suite ---"
	if len(argv) < 2:
		print "Error: no test executable given"
		return 1
	return run_tests(argv[1])

if __name__ == "__main__":
	exit(main())
