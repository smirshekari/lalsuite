//
//  Copyright (C) 2011, 2012 Karl Wette
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with with program; see the file COPYING. If not, write to the
//  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
//  MA  02111-1307  USA
//

// SWIG methods and operators for LAL GPS time class
// Author: Karl Wette

// Only in SWIG interface.
#if defined(SWIG) && !defined(SWIGXML)

// Specialised input typemaps for LIGOTimeGPS structs.
// Accepts a SWIG-wrapped LIGOTimeGPS or a double as input.
%typemap(in, noblock=1, fragment=SWIG_AsVal_frag(double))
  LIGOTimeGPS (void *argp = 0, int res = 0),
  const LIGOTimeGPS (void *argp = 0, int res = 0)
{
  res = SWIG_ConvertPtr($input, &argp, $&descriptor, $disown | %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    double val = 0;
    res = SWIG_AsVal(double)($input, &val);
    if (!SWIG_IsOK(res)) {
      %argument_fail(res, "$type", $symname, $argnum);
    } else {
      XLALGPSSetREAL8(&$1, val);
    }
  } else {
    if (!argp) {
      %argument_nullref("$type", $symname, $argnum);
    } else {
      $&ltype temp = %reinterpret_cast(argp, $&ltype);
      $1 = *temp;
      if (SWIG_IsNewObj(res)) {
        %delete(temp);
      }
    }
  }
}
%typemap(freearg) LIGOTimeGPS, const LIGOTimeGPS "";

// Specialised input typemaps for pointers to LIGOTimeGPS.
// Accepts a SWIG-wrapped LIGOTimeGPS or a double as input.
%typemap(in, noblock=1, fragment=SWIG_AsVal_frag(double))
  LIGOTimeGPS* (LIGOTimeGPS tmp, void *argp = 0, int res = 0),
  const LIGOTimeGPS* (LIGOTimeGPS tmp, void *argp = 0, int res = 0)
{
  res = SWIG_ConvertPtr($input, &argp, $descriptor, $disown | %convertptr_flags);
  if (!SWIG_IsOK(res)) {
    double val = 0;
    res = SWIG_AsVal(double)($input, &val);
    if (!SWIG_IsOK(res)) {
      %argument_fail(res, "$type", $symname, $argnum);
    } else {
      $1 = %reinterpret_cast(XLALGPSSetREAL8(&tmp, val), $ltype);
    }
  } else {
    $1 = %reinterpret_cast(argp, $ltype);
  }
}
%typemap(freearg) LIGOTimeGPS*, const LIGOTimeGPS* "";

// Allocate a new LIGOTimeGPS.
#define %lalswig_new_LIGOTimeGPS() (LIGOTimeGPS*)(XLALCalloc(1, sizeof(LIGOTimeGPS)))

// Extend the LIGOTimeGPS class.
// Uses some code from PyLAL: Copyright (C) 2006 Kipp Cannon
%extend tagLIGOTimeGPS {

  // Construct a new LIGOTimeGPS from a real number.
  tagLIGOTimeGPS(REAL8 t) {
    return XLALGPSSetREAL8(%lalswig_new_LIGOTimeGPS(), t);
  }

  // Construct a new LIGOTimeGPS from integer seconds and nanoseconds.
  tagLIGOTimeGPS(INT4 gpssec) {
    return XLALGPSSet(%lalswig_new_LIGOTimeGPS(), gpssec, 0);
  }
  tagLIGOTimeGPS(INT4 gpssec, INT8 gpsnan) {
    return XLALGPSSet(%lalswig_new_LIGOTimeGPS(), gpssec, gpsnan);
  }

  // Construct a new LIGOTimeGPS from a string
  tagLIGOTimeGPS(const char *str) {
    LIGOTimeGPS *gps = %lalswig_new_LIGOTimeGPS();
    char *end = NULL;
    if (XLALStrToGPS(gps, str, &end) < 0 || *end != '\0') {
      XLALFree(gps);
      xlalErrno = XLAL_EFUNC;   // Silently signal an error to constructor
      return NULL;
    }
    return gps;
  }

  // Operators are implemented by defining Python-style __operator__
  // methods (since LAL is C99, we don't have C++ operators available).
  // Many SWIG language modules will automatically map these functions
  // to scripting-language operations in their runtime code. In some
  // cases (ironically, Python itself when using -builtin!) additional
  // directives are needed in the scripting-language-specific interface.

  // Return new LIGOTimeGPSs which are the positive and negative values of $self.
  LIGOTimeGPS* __pos__() {
    return XLALINT8NSToGPS(%lalswig_new_LIGOTimeGPS(), +XLALGPSToINT8NS($self));
  }
  LIGOTimeGPS* __neg__() {
    return XLALINT8NSToGPS(%lalswig_new_LIGOTimeGPS(), -XLALGPSToINT8NS($self));
  }

  // Return a new LIGOTimeGPS which is the absolute value of $self.
  LIGOTimeGPS* __abs__() {
    return XLALINT8NSToGPS(%lalswig_new_LIGOTimeGPS(), llabs(XLALGPSToINT8NS($self)));
  }

  // Return whether a LIGOTimeGPS is non-zero.
  bool __nonzero__() {
    return $self->gpsSeconds || $self->gpsNanoSeconds;
  }

  // Return integer representations of the LIGOTimeGPS seconds.
  int __int__() {
    return $self->gpsSeconds;
  }
  long __long__() {
    return $self->gpsSeconds;
  }

  // Return a floating-point representation of a LIGOTimeGPS.
  double __float__() {
    return XLALGPSGetREAL8($self);
  }

  // Return a string representation of a LIGOTimeGPS. Because
  // XLALGPSToStr() allocates a new string using LAL memory,
  // %newobject is used to make SWIG use a 'newfree' typemap,
  // where the string is freed; SWIG will have already copied it
  // to a native scripting-language string to return as output.
  %newobject __str__;
  %typemap(newfree) char* __str__ "XLALFree($1);";
  char* __str__() {
    return XLALGPSToStr(NULL, $self);
  }
  %newobject __repr__;
  %typemap(newfree) char* __repr__ "XLALFree($1);";
  char* __repr__() {
    return XLALGPSToStr(NULL, $self);
  }

  // Return the hash value of a LIGOTimeGPS.
  long __hash__() {
    long hash = (long)$self->gpsSeconds ^ (long)$self->gpsNanoSeconds;
    return hash == -1 ? -2 : hash;
  }

  // Additive binary operators are generated by the following SWIG macro.
  // NAME is the name of the Python operator, and OPFUNC(lhs,rhs) is the
  // name of a function that performs the operation on two LIGOTimeGPSs,
  // lhs and rhs, and returns the (modified) lhs.
  %define %lalswig_LIGOTimeGPS_additive_operator(NAME, OPFUNC)

    // Perform op. between two LIGOTimeGPSs, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __##NAME##__(LIGOTimeGPS* gps) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      *retn = *$self;
      return OPFUNC(retn, gps);
    }

    // Perform op. between a LIGOTimeGPS and an INT4, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __##NAME##__(INT4 t) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      *retn = *$self;
      LIGOTimeGPS tgps;
      XLALGPSSet(&tgps, t, 0);
      return OPFUNC(retn, &tgps);
    }

    // Perform op. between an INT4 and a LIGOTimeGPS, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __r##NAME##__(INT4 t) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      XLALGPSSet(retn, t, 0);
      return OPFUNC(retn, $self);
    }

    // Perform op. between a LIGOTimeGPS and a REAL8, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __##NAME##__(REAL8 t) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      *retn = *$self;
      LIGOTimeGPS tgps;
      XLALGPSSetREAL8(&tgps, t);
      return OPFUNC(retn, &tgps);
    }

    // Perform op. between a REAL8 and a LIGOTimeGPS, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __r##NAME##__(REAL8 t) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      XLALGPSSetREAL8(retn, t);
      return OPFUNC(retn, $self);
    }

  %enddef // %lalswig_LIGOTimeGPS_additive_operator

  // Addition
  %lalswig_LIGOTimeGPS_additive_operator(add, XLALGPSAddGPS);

  // Subtraction
  #define %lalswig_LIGOTimeGPS_sub(LHS,RHS) XLALGPSSetREAL8(LHS, XLALGPSDiff(LHS, RHS))
  %lalswig_LIGOTimeGPS_additive_operator(sub, %lalswig_LIGOTimeGPS_sub);

  // Multiplicative binary operators are generated by the following SWIG macro.
  // NAME is the name of the Python operator, and OPFUNC(lhs,rhs) is the name
  // of a function that performs the operation between a LIGOTimeGPS, lhs, and
  // a REAL 8, rhs, and returns the (modified) lhs.
  %define %lalswig_LIGOTimeGPS_multiplicative_operator(NAME, OPFUNC)

    // Perform op. between two LIGOTimeGPSs, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __##NAME##__(LIGOTimeGPS* gps) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      *retn = *$self;
      return OPFUNC(retn, XLALGPSGetREAL8(gps));
    }

    // Perform op. between a LIGOTimeGPS and a REAL8, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __##NAME##__(REAL8 t) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      *retn = *$self;
      return OPFUNC(retn, t);
    }

    // Perform op. between a REAL8 and a LIGOTimeGPS, and return the result as a new LIGOTimeGPS.
    LIGOTimeGPS* __r##NAME##__(REAL8 t) {
      LIGOTimeGPS* retn = %lalswig_new_LIGOTimeGPS();
      XLALGPSSetREAL8(retn, t);
      return OPFUNC(retn, XLALGPSGetREAL8($self));
    }

  %enddef // %lalswig_LIGOTimeGPS_multiplicative_operator

  // Multiplication
  %lalswig_LIGOTimeGPS_multiplicative_operator(mul, XLALGPSMultiply);

  // Floating-point division
  %lalswig_LIGOTimeGPS_multiplicative_operator(div, XLALGPSDivide);

  // Modulus
  #define %lalswig_LIGOTimeGPS_mod(LHS,RHS) XLALGPSSetREAL8(LHS, fmod(XLALGPSGetREAL8(LHS), RHS))
  %lalswig_LIGOTimeGPS_multiplicative_operator(mod, %lalswig_LIGOTimeGPS_mod);

  // Integer division
  #define %lalswig_LIGOTimeGPS_floordiv(LHS,RHS) XLALGPSSetREAL8(LHS, floor(XLALGPSGetREAL8(XLALGPSDivide(LHS, RHS))))
  %lalswig_LIGOTimeGPS_multiplicative_operator(floordiv, %lalswig_LIGOTimeGPS_floordiv);

  // Comparison operators are generated by the following SWIG macro.
  // NAME is the name of the Python operator and OP is the C operator.
  // The correct comparison NAME is obtained by comparing the result
  // of the result of XLALGPSCmp() against zero using OP.
  %define %lalswig_LIGOTimeGPS_comparison_operator(NAME, OP)

    // Compare two LIGOTimeGPSs
    bool __##NAME##__(LIGOTimeGPS *gps) {
      return XLALGPSCmp($self, gps) OP 0;
    }

    // Compare a LIGOTimeGPS and an INT4
    bool __##NAME##__(INT4 t) {
      LIGOTimeGPS tmp;
      return XLALGPSCmp($self, XLALGPSSet(&tmp, t, 0)) OP 0;
    }

    // Compare a LIGOTimeGPS and a REAL8
    bool __##NAME##__(REAL8 t) {
      LIGOTimeGPS tmp;
      return XLALGPSCmp($self, XLALGPSSetREAL8(&tmp, t)) OP 0;
    }

  %enddef

  // Comparison operators
  %lalswig_LIGOTimeGPS_comparison_operator(lt, < );
  %lalswig_LIGOTimeGPS_comparison_operator(le, <=);
  %lalswig_LIGOTimeGPS_comparison_operator(eq, ==);
  %lalswig_LIGOTimeGPS_comparison_operator(ne, !=);
  %lalswig_LIGOTimeGPS_comparison_operator(gt, > );
  %lalswig_LIGOTimeGPS_comparison_operator(ge, >=);

  // Return the number of nanoseconds in a LIGOTimeGPS
  INT8 ns() {
    return XLALGPSToINT8NS($self);
  }

} // %extend tagLIGOTimeGPS

#endif // SWIG
