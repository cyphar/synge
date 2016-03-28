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

([GPLv3 or later](https://www.gnu.org/licenses/gpl-3.0.en.html))

```
Copyright (C) 2013, 2016 Aleksa Sarai

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
```
