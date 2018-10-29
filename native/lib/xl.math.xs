// ****************************************************************************
//  xl.math.xs                                      XL - An extensible language
// ****************************************************************************
//
//   File Description:
//
//     Basic math functions
//
//
//
//
//
//
//
//
// ****************************************************************************
//  (C) 2018 Christophe de Dinechin <christophe@dinechin.org>
//   This software is licensed under the GNU General Public License v3
//   See LICENSE file for details.
// ****************************************************************************

module XL.MATH with

    // Basic math functions
    abs   X:integer     as integer      is builtin IAbs
    abs   X:real        as real         is builtin FAbs
    sqrt  X:real        as real         is C sqrt

    sin   X:real        as real         is C sin
    cos   X:real        as real         is C cos
    tan   X:real        as real         is C tan
    asin  X:real        as real         is C asin
    acos  X:real        as real         is C acos
    atan  X:real        as real         is C atan

    sinh  X:real        as real         is C sinh
    cosh  X:real        as real         is C cosh
    tanh  X:real        as real         is C tanh
    asinh X:real        as real         is C asinh
    acosh X:real        as real         is C acosh
    atanh X:real        as real         is C atanh

    exp   X:real        as real         is C exp
    expm1 X:real        as real         is C expm1
    log   X:real        as real         is C log
    log10 X:real        as real         is C log10
    log2  X:real        as real         is C log2
    log1p X:real        as real         is C log1p

    pi                                  is 3.1415926535897932384626433
