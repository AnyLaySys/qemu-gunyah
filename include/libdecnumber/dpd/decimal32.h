

#ifndef DECIMAL32_H
#define DECIMAL32_H

  #define DEC32NAME	"decimal32"		      /* Short name   */
  #define DEC32FULLNAME "Decimal 32-bit Number"	      /* Verbose name */
  #define DEC32AUTHOR	"Mike Cowlishaw"	      /* Who to blame */

  #define DECIMAL32_Bytes  4		/* length		      */
  #define DECIMAL32_Pmax   7		/* maximum precision (digits) */
  #define DECIMAL32_Emax   96		/* maximum adjusted exponent  */
  #define DECIMAL32_Emin  -95		/* minimum adjusted exponent  */
  #define DECIMAL32_Bias   101		/* bias for the exponent      */
  #define DECIMAL32_String 15		/* maximum string length, +1  */
  #define DECIMAL32_EconL  6		/* exp. continuation length   */
  #define DECIMAL32_Ehigh  (DECIMAL32_Emax+DECIMAL32_Bias-DECIMAL32_Pmax+1)

  #if defined(DECNUMDIGITS)
    #if (DECNUMDIGITS<DECIMAL32_Pmax)
      #error decimal32.h needs pre-defined DECNUMDIGITS>=7 for safe use
    #endif
  #endif

  #ifndef DECNUMDIGITS
    #define DECNUMDIGITS DECIMAL32_Pmax /* size if not already defined*/
  #endif
  #include "libdecnumber/decNumber.h"

  typedef struct {
    uint8_t bytes[DECIMAL32_Bytes];	/* decimal32: 1, 5, 6, 20 bits*/
    } decimal32;

  #if !defined(DECIMAL_NaN)
    #define DECIMAL_NaN	    0x7c	/* 0 11111 00 NaN	      */
    #define DECIMAL_sNaN    0x7e	/* 0 11111 10 sNaN	      */
    #define DECIMAL_Inf	    0x78	/* 0 11110 00 Infinity	      */
  #endif



  decimal32 * decimal32FromString(decimal32 *, const char *, decContext *);
  char * decimal32ToString(const decimal32 *, char *);
  char * decimal32ToEngString(const decimal32 *, char *);

  decimal32 * decimal32FromNumber(decimal32 *, const decNumber *,
				  decContext *);
  decNumber * decimal32ToNumber(const decimal32 *, decNumber *);

  uint32_t    decimal32IsCanonical(const decimal32 *);
  decimal32 * decimal32Canonical(decimal32 *, const decimal32 *);

#endif
