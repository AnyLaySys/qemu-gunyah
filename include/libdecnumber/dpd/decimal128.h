

#ifndef DECIMAL128_H
#define DECIMAL128_H

  #define DEC128NAME	 "decimal128"		      /* Short name   */
  #define DEC128FULLNAME "Decimal 128-bit Number"     /* Verbose name */
  #define DEC128AUTHOR	 "Mike Cowlishaw"	      /* Who to blame */

  #define DECIMAL128_Bytes  16		/* length		      */
  #define DECIMAL128_Pmax   34		/* maximum precision (digits) */
  #define DECIMAL128_Emax   6144	/* maximum adjusted exponent  */
  #define DECIMAL128_Emin  -6143	/* minimum adjusted exponent  */
  #define DECIMAL128_Bias   6176	/* bias for the exponent      */
  #define DECIMAL128_String 43		/* maximum string length, +1  */
  #define DECIMAL128_EconL  12		/* exp. continuation length   */
  #define DECIMAL128_Ehigh  (DECIMAL128_Emax+DECIMAL128_Bias-DECIMAL128_Pmax+1)

  #if defined(DECNUMDIGITS)
    #if (DECNUMDIGITS<DECIMAL128_Pmax)
      #error decimal128.h needs pre-defined DECNUMDIGITS>=34 for safe use
    #endif
  #endif

  #ifndef DECNUMDIGITS
    #define DECNUMDIGITS DECIMAL128_Pmax /* size if not already defined*/
  #endif
  #include "libdecnumber/decNumber.h"

  typedef struct {
    uint8_t bytes[DECIMAL128_Bytes]; /* decimal128: 1, 5, 12, 110 bits*/
    } decimal128;

  #if !defined(DECIMAL_NaN)
    #define DECIMAL_NaN	    0x7c	/* 0 11111 00 NaN	      */
    #define DECIMAL_sNaN    0x7e	/* 0 11111 10 sNaN	      */
    #define DECIMAL_Inf	    0x78	/* 0 11110 00 Infinity	      */
  #endif

  #include "decimal128Local.h"



  decimal128 * decimal128FromString(decimal128 *, const char *, decContext *);
  char * decimal128ToString(const decimal128 *, char *);
  char * decimal128ToEngString(const decimal128 *, char *);

  decimal128 * decimal128FromNumber(decimal128 *, const decNumber *,
				    decContext *);
  decNumber * decimal128ToNumber(const decimal128 *, decNumber *);

  uint32_t    decimal128IsCanonical(const decimal128 *);
  decimal128 * decimal128Canonical(decimal128 *, const decimal128 *);

#endif
