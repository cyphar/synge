Synge
-----
[![Build Status](https://travis-ci.org/cyphar/synge.png?branch=master)](https://travis-ci.org/cyphar/synge)

A **Shunting Yard calculation eNGinE** with GTK+ and Command Line wrappers.

## Features ##
* Multiple Precision Numbers (using MPFR)
* Scientific Functions
* Mathematical Constants
* Recursive User Functions
* Variables
* Simple Programming API
* Detailed Traceback Errors
* Dynamic Scoping

## Examples ##
Fibonacci Sequence:
* `a = b = 1` (set up variables)
* `(a < b) ? (a = a + b) : (b = a + b)` (generate next number in sequence)

Series:
* `x = <input>` (input)
* `f := (x > 0) ? x-- + f : 0` (recursively generate result)

Factorial:
* `x = <input>` (input)
* `f := (x > 0) ? x-- * f : 1` (recursively generate result)

Traceback:
* `f := 1/0`
* `f`
```
Synge Traceback (most recent call last):
  Function <main>, at 0
  Function f, at 1
MathError: Cannot divide by zero @ 2
```

## License ##
Synge is [MIT Licenced](http://opensource.org/licenses/mit-license), meaning that it can be used in both open source and propretary projects (provided the license conditions are met).
```
Copyright (c) 2013 Cyphar

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

1. The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
