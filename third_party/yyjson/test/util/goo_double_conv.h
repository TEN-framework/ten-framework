#ifndef goo_double_conv_h
#define goo_double_conv_h

#include <float.h>

/// IEEE 754 floating-point binary representation detection.
/// The functions below may produce incorrect results if GOO_HAS_IEEE_754 == 0.
#if defined(__STDC_IEC_559__) || defined(__STDC_IEC_60559_BFP__)
#   define GOO_HAS_IEEE_754 1
#elif (FLT_RADIX == 2) && (DBL_MANT_DIG == 53) && (DBL_DIG == 15) && \
     (DBL_MIN_EXP == -1021) && (DBL_MAX_EXP == 1024) && \
     (DBL_MIN_10_EXP == -307) && (DBL_MAX_10_EXP == 308)
#   define GOO_HAS_IEEE_754 1
#else
#   define GOO_HAS_IEEE_754 0
#endif

/// Convert double number to shortest string (with null-terminator).
/// The string format follows the ECMAScript spec with the following changes:
/// 1. Keep the negative sign of 0.0 to preserve input information.
/// 2. Keep decimal point to indicate the number is floating point.
/// 3. Remove positive sign of exponent part.
/// @param val A double value.
/// @param buf A string buffer to receive output.
/// @param len The string buffer length.
/// @return The string length, or 0 if failed.
int goo_dtoa(double val, char *buf, int len);

/// Convert double number to string with precision (with null-terminator).
/// The string format follows the ECMAScript spec with the following changes:
/// 1. Keep the negative sign of 0.0 to preserve input information.
/// 2. Keep decimal point to indicate the number is floating point.
/// 3. Remove positive sign of exponent part.
/// @param val A double value.
/// @param prec Max precision kept by string, should in range [1, 120].
/// @param buf A string buffer to receive output.
/// @param len The string buffer length.
/// @return The string length, or 0 if failed.
int goo_dtoa_prec(double val, int prec, char *buf, int len);

/// Read double number from string, support same format as libc's strtod().
/// @param str A string with double number.
/// @param len In: the string length. Out: the processed length, or 0 if failed.
/// @return The double value, or 0.0 if failed.
double goo_strtod(const char *str, int *len);

#endif /* goo_double_conv_h */
