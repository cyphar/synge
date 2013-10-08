Synge
-----
[![Build Status](https://travis-ci.org/cyphar/synge.png?branch=master)](https://travis-ci.org/cyphar/synge)

A **Shunting Yard calculation eNGinE** with GTK+ and Command Line interfaces.

## Features ##
* Multiple Precision Numbers (using MPFR + GMP)
  - Correct to 64 decimal places
  - No IEEE 754 rounding errors
* Scientific Functions
  - `abs(...)`
  - `sqrt(...)`
  - `cbrt(...)`
  - `round(...)`
  - `ceil(...)`
  - `floor(...)`
  - `log(...)`
  - `ln(...)`
  - `log10(...)`
  - `rand(...)`
  - `randi(...)`
  - `fact(...)`
  - `series(...)`
  - `bool(...)`
  - `deg2rad(...)`
  - `deg2grad(...)`
  - `rad2deg(...)`
  - `rad2grad(...)`
  - `grad2deg(...)`
  - `grad2rad(...)`
  - `sinh(...)`
  - `cosh(...)`
  - `tanh(...)`
  - `asinh(...)`
  - `acosh(...)`
  - `atanh(...)`
  - `sin(...)`
  - `cos(...)`
  - `tan(...)`
  - `asin(...)`
  - `acos(...)`
  - `atan(...)`
* Shortcut Conditional Expressions
  - `true ? 42 : 1` (`42`)
  - `false ? 1/0 : 3` (`3`, no error)
* Mathematical Constants
  - `pi`
  - `e`
  - `phi`
  - `true`
  - `false`
* Recursive User Functions
  - `f := f + 1`
* Variables
  - `a = 42`
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
* `f := 1/0` (will not print an error)
* `true ? f : 0`

```
Synge Traceback (most recent call last):
  Module <main>
  <if> condition, at 6
  Function f, at 1
MathError: Cannot divide by zero @ 2
```

Dynamic Scoping:
* `f := a=3`
* `::a` (delete a)
* `f` (define a within f)
* `a`

## License ##
Synge is [MIT Licenced](http://opensource.org/licenses/mit-license).
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
