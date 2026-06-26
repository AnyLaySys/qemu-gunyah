

#ifndef DECNUMBER_H
#define DECNUMBER_H

  #define DECNAME     "decNumber"			/* Short name */
  #define DECFULLNAME "Decimal Number Module"	      /* Verbose name */
  #define DECAUTHOR   "Mike Cowlishaw"		      /* Who to blame */

  #include "libdecnumber/decContext.h"

  #define DECNEG    0x80      /* Sign; 1=negative, 0=positive or zero */
  #define DECINF    0x40      /* 1=Infinity			      */
  #define DECNAN    0x20      /* 1=NaN				      */
  #define DECSNAN   0x10      /* 1=sNaN				      */
  #define DECSPECIAL (DECINF|DECNAN|DECSNAN) /* any special value     */


  #define DECDPUN 3	      /* DECimal Digits Per UNit [must be >0  */

  #if !defined(DECNUMDIGITS)
    #define DECNUMDIGITS 1
  #endif

  #if	DECDPUN<=2
    #define decNumberUnit uint8_t
  #elif DECDPUN<=4
    #define decNumberUnit uint16_t
  #else
    #define decNumberUnit uint32_t
  #endif
  #define DECNUMUNITS ((DECNUMDIGITS+DECDPUN-1)/DECDPUN)

  typedef struct {
    int32_t digits;	 /* Count of digits in the coefficient; >0    */
    int32_t exponent;	 /* Unadjusted exponent, unbiased, in	      */
    uint8_t bits;	 /* Indicator bits (see above)		      */
    decNumberUnit lsu[DECNUMUNITS];
    } decNumber;





  decNumber * decNumberFromInt32(decNumber *, int32_t);
  decNumber * decNumberFromUInt32(decNumber *, uint32_t);
  decNumber *decNumberFromInt64(decNumber *, int64_t);
  decNumber *decNumberFromUInt64(decNumber *, uint64_t);
  decNumber *decNumberFromInt128(decNumber *, uint64_t, int64_t);
  decNumber *decNumberFromUInt128(decNumber *, uint64_t, uint64_t);
  decNumber * decNumberFromString(decNumber *, const char *, decContext *);
  char	    * decNumberToString(const decNumber *, char *);
  char	    * decNumberToEngString(const decNumber *, char *);
  uint32_t    decNumberToUInt32(const decNumber *, decContext *);
  int32_t     decNumberToInt32(const decNumber *, decContext *);
  int64_t     decNumberIntegralToInt64(const decNumber *dn, decContext *set);
  void        decNumberIntegralToInt128(const decNumber *dn, decContext *set,
        uint64_t *plow, uint64_t *phigh);
  uint8_t   * decNumberGetBCD(const decNumber *, uint8_t *);
  decNumber * decNumberSetBCD(decNumber *, const uint8_t *, uint32_t);

  decNumber * decNumberAbs(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberAdd(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberAnd(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberCompare(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberCompareSignal(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberCompareTotal(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberCompareTotalMag(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberDivide(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberDivideInteger(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberExp(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberFMA(decNumber *, const decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberInvert(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberLn(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberLogB(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberLog10(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberMax(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberMaxMag(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberMin(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberMinMag(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberMinus(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberMultiply(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberNormalize(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberOr(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberPlus(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberPower(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberQuantize(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberReduce(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberRemainder(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberRemainderNear(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberRescale(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberRotate(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberSameQuantum(decNumber *, const decNumber *, const decNumber *);
  decNumber * decNumberScaleB(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberShift(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberSquareRoot(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberSubtract(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber * decNumberToIntegralExact(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberToIntegralValue(decNumber *, const decNumber *, decContext *);
  decNumber * decNumberXor(decNumber *, const decNumber *, const decNumber *, decContext *);

  enum decClass decNumberClass(const decNumber *, decContext *);
  const char * decNumberClassToString(enum decClass);
  decNumber  * decNumberCopy(decNumber *, const decNumber *);
  decNumber  * decNumberCopyAbs(decNumber *, const decNumber *);
  decNumber  * decNumberCopyNegate(decNumber *, const decNumber *);
  decNumber  * decNumberCopySign(decNumber *, const decNumber *, const decNumber *);
  decNumber  * decNumberNextMinus(decNumber *, const decNumber *, decContext *);
  decNumber  * decNumberNextPlus(decNumber *, const decNumber *, decContext *);
  decNumber  * decNumberNextToward(decNumber *, const decNumber *, const decNumber *, decContext *);
  decNumber  * decNumberTrim(decNumber *);
  const char * decNumberVersion(void);
  decNumber  * decNumberZero(decNumber *);

  int32_t decNumberIsNormal(const decNumber *, decContext *);
  int32_t decNumberIsSubnormal(const decNumber *, decContext *);

  #define decNumberIsCanonical(dn) (1)	/* All decNumbers are saintly */
  #define decNumberIsFinite(dn)	   (((dn)->bits&DECSPECIAL)==0)
  #define decNumberIsInfinite(dn)  (((dn)->bits&DECINF)!=0)
  #define decNumberIsNaN(dn)	   (((dn)->bits&(DECNAN|DECSNAN))!=0)
  #define decNumberIsNegative(dn)  (((dn)->bits&DECNEG)!=0)
  #define decNumberIsQNaN(dn)	   (((dn)->bits&(DECNAN))!=0)
  #define decNumberIsSNaN(dn)	   (((dn)->bits&(DECSNAN))!=0)
  #define decNumberIsSpecial(dn)   (((dn)->bits&DECSPECIAL)!=0)
  #define decNumberIsZero(dn)	   (*(dn)->lsu==0 \
				    && (dn)->digits==1 \
				    && (((dn)->bits&DECSPECIAL)==0))
  #define decNumberRadix(dn)	   (10)

#endif
