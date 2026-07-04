

#ifndef DECIMAL64_H
#define DECIMAL64_H

  #define DEC64NAME	"decimal64"		      /* Short name   */
  #define DEC64FULLNAME "Decimal 64-bit Number"	      /* Verbose name */
  #define DEC64AUTHOR	"Mike Cowlishaw"	      /* Who to blame */


  #define DECIMAL64_Bytes  8		/* length		      */
  #define DECIMAL64_Pmax   16		/* maximum precision (digits) */
  #define DECIMAL64_Emax   384		/* maximum adjusted exponent  */
  #define DECIMAL64_Emin  -383		/* minimum adjusted exponent  */
  #define DECIMAL64_Bias   398		/* bias for the exponent      */
  #define DECIMAL64_String 24		/* maximum string length, +1  */
  #define DECIMAL64_EconL  8		/* exp. continuation length   */
  #define DECIMAL64_Ehigh  (DECIMAL64_Emax+DECIMAL64_Bias-DECIMAL64_Pmax+1)

  #if defined(DECNUMDIGITS)
    #if (DECNUMDIGITS<DECIMAL64_Pmax)
      #error decimal64.h needs pre-defined DECNUMDIGITS>=16 for safe use
    #endif
  #endif


  #ifndef DECNUMDIGITS
    #define DECNUMDIGITS DECIMAL64_Pmax /* size if not already defined*/
  #endif
  #include "libdecnumber/decNumber.h"

  typedef struct {
    uint8_t bytes[DECIMAL64_Bytes];	/* decimal64: 1, 5, 8, 50 bits*/
    } decimal64;

  #if !defined(DECIMAL_NaN)
    #define DECIMAL_NaN	    0x7c	/* 0 11111 00 NaN	      */
    #define DECIMAL_sNaN    0x7e	/* 0 11111 10 sNaN	      */
    #define DECIMAL_Inf	    0x78	/* 0 11110 00 Infinity	      */
  #endif



  decimal64 * decimal64FromString(decimal64 *, const char *, decContext *);
  char * decimal64ToString(const decimal64 *, char *);
  char * decimal64ToEngString(const decimal64 *, char *);

  decimal64 * decimal64FromNumber(decimal64 *, const decNumber *,
				  decContext *);
  decNumber * decimal64ToNumber(const decimal64 *, decNumber *);

  uint32_t    decimal64IsCanonical(const decimal64 *);
  decimal64 * decimal64Canonical(decimal64 *, const decimal64 *);

#endif
