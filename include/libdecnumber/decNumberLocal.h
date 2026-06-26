

#ifndef DECNUMBERLOCAL_H
#define DECNUMBERLOCAL_H

  #define DECVERSION	"decNumber 3.53" /* Package Version [16 max.] */
  #define DECNLAUTHOR	"Mike Cowlishaw"	      /* Who to blame */

  #include "libdecnumber/dconfig.h"
  #include "libdecnumber/decContext.h"

  #if WORDS_BIGENDIAN
  #define DECLITEND 0
  #else
  #define DECLITEND 1
  #endif

  #define DECUSE64  1	      /* 1=use int64s, 0=int32 & smaller only */

  #define DECCHECK  0	      /* 1 to enable robust checking	      */
  #define DECALLOC  0	      /* 1 to enable memory accounting	      */
  #define DECTRACE  0	      /* 1 to trace certain internals, etc.   */

  #define DECBUFFER 36	      /* Size basis for local buffers.	This  */


  #define Flag	 uint8_t
  #define Byte	 int8_t
  #define uByte	 uint8_t
  #define Short	 int16_t
  #define uShort uint16_t
  #define Int	 int32_t
  #define uInt	 uint32_t
  #define Unit	 decNumberUnit
  #if DECUSE64
  #define Long	 int64_t
  #define uLong	 uint64_t
  #endif

  typedef long int LI;	      /* for printf arguments only	      */
  #define DECNOINT  0	      /* 1 to check no internal use of 'int'  */
  #if DECNOINT
    #define  int ?	      /* enable to ensure that plain C 'int'  */
    #define  long ??	      /* .. or 'long' types are not used      */
  #endif

  extern const uByte  DECSTICKYTAB[10]; /* re-round digits if sticky  */
  extern const uLong  DECPOWERS[20];    /* powers of ten table        */
  extern const uShort DPD2BIN[1024];	/* DPD -> 0-999		      */
  extern const uShort BIN2DPD[1000];	/* 0-999 -> DPD		      */
  extern const uInt   DPD2BINK[1024];	/* DPD -> 0-999000	      */
  extern const uInt   DPD2BINM[1024];	/* DPD -> 0-999000000	      */
  extern const uByte  DPD2BCD8[4096];	/* DPD -> ddd + len	      */
  extern const uByte  BIN2BCD8[4000];	/* 0-999 -> ddd + len	      */
  extern const uShort BCD2DPD[2458];	/* 0-0x999 -> DPD (0x999=2457)*/

  #define LONGMUL32HI(w, u, v) {	     \
    uInt u0, u1, v0, v1, w0, w1, w2, t;	     \
    u0=u & 0xffff; u1=u>>16;		     \
    v0=v & 0xffff; v1=v>>16;		     \
    w0=u0*v0;				     \
    t=u1*v0 + (w0>>16);			     \
    w1=t & 0xffff; w2=t>>16;		     \
    w1=u0*v1 + w1;			     \
    (w)=u1*v1 + w2 + (w1>>16);}

  #define ROUNDUP(i, n) ((((i)+(n)-1)/n)*n)

  #define ROUNDDOWN(i, n) (((i)/n)*n)
  #define ROUNDDOWN4(i)	  ((i)&~3)	/* special for n=4	      */

  #define UINTAT(b)   (*((uInt	 *)(b)))
  #define USHORTAT(b) (*((uShort *)(b)))
  #define UBYTEAT(b)  (*((uByte	 *)(b)))

  #define X10(i)  (((i)<<1)+((i)<<3))
  #define X100(i) (((i)<<2)+((i)<<5)+((i)<<6))

  #define MAXI(x,y) ((x)<(y)?(y):(x))
  #define MINI(x,y) ((x)>(y)?(y):(x))

  #define BILLION      1000000000	     /* 10**9		      */
  #define CHARMASK ((((((((uInt)'0')<<8)+'0')<<8)+'0')<<8)+'0')



  #define DECNUMMAXP 999999999	/* maximum precision code can handle  */
  #define DECNUMMAXE 999999999	/* maximum adjusted exponent ditto    */
  #define DECNUMMINE -999999999 /* minimum adjusted exponent ditto    */
  #if (DECNUMMAXP != DEC_MAX_DIGITS)
    #error Maximum digits mismatch
  #endif
  #if (DECNUMMAXE != DEC_MAX_EMAX)
    #error Maximum exponent mismatch
  #endif
  #if (DECNUMMINE != DEC_MIN_EMIN)
    #error Minimum exponent mismatch
  #endif

  #if	DECDPUN==1
    #define DECDPUNMAX 9
    #define D2UTABLE {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,  \
		      18,19,20,21,22,23,24,25,26,27,28,29,30,31,32, \
		      33,34,35,36,37,38,39,40,41,42,43,44,45,46,47, \
		      48,49}
  #elif DECDPUN==2
    #define DECDPUNMAX 99
    #define D2UTABLE {0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,  \
		      11,11,12,12,13,13,14,14,15,15,16,16,17,17,18, \
		      18,19,19,20,20,21,21,22,22,23,23,24,24,25}
  #elif DECDPUN==3
    #define DECDPUNMAX 999
    #define D2UTABLE {0,1,1,1,2,2,2,3,3,3,4,4,4,5,5,5,6,6,6,7,7,7,  \
		      8,8,8,9,9,9,10,10,10,11,11,11,12,12,12,13,13, \
		      13,14,14,14,15,15,15,16,16,16,17}
  #elif DECDPUN==4
    #define DECDPUNMAX 9999
    #define D2UTABLE {0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,6,  \
		      6,6,6,7,7,7,7,8,8,8,8,9,9,9,9,10,10,10,10,11, \
		      11,11,11,12,12,12,12,13}
  #elif DECDPUN==5
    #define DECDPUNMAX 99999
    #define D2UTABLE {0,1,1,1,1,1,2,2,2,2,2,3,3,3,3,3,4,4,4,4,4,5,  \
		      5,5,5,5,6,6,6,6,6,7,7,7,7,7,8,8,8,8,8,9,9,9,  \
		      9,9,10,10,10,10}
  #elif DECDPUN==6
    #define DECDPUNMAX 999999
    #define D2UTABLE {0,1,1,1,1,1,1,2,2,2,2,2,2,3,3,3,3,3,3,4,4,4,  \
		      4,4,4,5,5,5,5,5,5,6,6,6,6,6,6,7,7,7,7,7,7,8,  \
		      8,8,8,8,8,9}
  #elif DECDPUN==7
    #define DECDPUNMAX 9999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,2,2,2,2,2,2,2,3,3,3,3,3,3,3,  \
		      4,4,4,4,4,4,4,5,5,5,5,5,5,5,6,6,6,6,6,6,6,7,  \
		      7,7,7,7,7,7}
  #elif DECDPUN==8
    #define DECDPUNMAX 99999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,3,3,3,3,3,  \
		      3,3,3,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,  \
		      6,6,6,6,6,7}
  #elif DECDPUN==9
    #define DECDPUNMAX 999999999
    #define D2UTABLE {0,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,3,3,3,  \
		      3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,  \
		      5,5,6,6,6,6}
  #elif defined(DECDPUN)
    #error DECDPUN must be in the range 1-9
  #endif

  #define DECMAXD2U 49
  extern const uByte d2utable[DECMAXD2U+1];

  #define ISZERO(dn) decNumberIsZero(dn)     /* now just a local name */

  #if DECDPUN==8
    #define D2U(d) ((unsigned)((d)<=DECMAXD2U?d2utable[d]:((d)+7)>>3))
  #elif DECDPUN==4
    #define D2U(d) ((unsigned)((d)<=DECMAXD2U?d2utable[d]:((d)+3)>>2))
  #else
    #define D2U(d) ((d)<=DECMAXD2U?d2utable[d]:((d)+DECDPUN-1)/DECDPUN)
  #endif
  #define SD2U(d) (((d)+DECDPUN-1)/DECDPUN)

  #define MSUDIGITS(d) ((d)-(D2U(d)-1)*DECDPUN)

  #define D2N(d) \
    ((((SD2U(d)-1)*sizeof(Unit))+sizeof(decNumber)*2-1)/sizeof(decNumber))

  #define TODIGIT(u, cut, c, pow) {	  \
    *(c)='0';				  \
    pow=DECPOWERS[cut]*2;		  \
    if ((u)>pow) {			  \
      pow*=4;				  \
      if ((u)>=pow) {(u)-=pow; *(c)+=8;}  \
      pow/=2;				  \
      if ((u)>=pow) {(u)-=pow; *(c)+=4;}  \
      pow/=2;				  \
      }					  \
    if ((u)>=pow) {(u)-=pow; *(c)+=2;}	  \
    pow/=2;				  \
    if ((u)>=pow) {(u)-=pow; *(c)+=1;}	  \
    }


  typedef struct {
    uByte   *msd;	      /* -> most significant digit	      */
    uByte   *lsd;	      /* -> least ditto			      */
    uInt     sign;	      /* 0=positive, DECFLOAT_Sign=negative   */
    Int	     exponent;	      /* Unadjusted signed exponent (q), or   */
    } bcdnum;

  #define EXPISSPECIAL(exp) ((exp)>=DECFLOAT_MinSp)
  #define EXPISINF(exp) (exp==DECFLOAT_Inf)
  #define EXPISNAN(exp) (exp==DECFLOAT_qNaN || exp==DECFLOAT_sNaN)
  #define NUMISSPECIAL(num) (EXPISSPECIAL((num)->exponent))

  #define DECWORDS  (DECBYTES/4)
  #define DECWWORDS (DECWBYTES/4)
  #if DECLITEND
    #define DFWORD(df, off) ((df)->words[DECWORDS-1-(off)])
    #define DFBYTE(df, off) ((df)->bytes[DECBYTES-1-(off)])
    #define DFWWORD(dfw, off) ((dfw)->words[DECWWORDS-1-(off)])
  #else
    #define DFWORD(df, off) ((df)->words[off])
    #define DFBYTE(df, off) ((df)->bytes[off])
    #define DFWWORD(dfw, off) ((dfw)->words[off])
  #endif

  #define DFISSIGNED(df)   (DFWORD(df, 0)&0x80000000)
  #define DFISSPECIAL(df) ((DFWORD(df, 0)&0x78000000)==0x78000000)
  #define DFISINF(df)	  ((DFWORD(df, 0)&0x7c000000)==0x78000000)
  #define DFISNAN(df)	  ((DFWORD(df, 0)&0x7c000000)==0x7c000000)
  #define DFISQNAN(df)	  ((DFWORD(df, 0)&0x7e000000)==0x7c000000)
  #define DFISSNAN(df)	  ((DFWORD(df, 0)&0x7e000000)==0x7e000000)

  extern const uInt   DECCOMBMSD[64];	/* Combination field -> MSD   */
  extern const uInt   DECCOMBFROM[48];	/* exp+msd -> Combination     */

  #if DECCHECK || DECTRACE
    extern void decShowNum(const bcdnum *, const char *);
  #endif

  #if defined(DECPMAX)

    #define DECPMAX9  (ROUNDUP(DECPMAX, 9)/9)  /* 'Pmax' in 10**9s    */
    #define SINGLEZERO	 0x22500000
    #define DOUBLEZERO	 0x22380000
    #define QUADZERO	 0x22080000

    #if DECPMAX==7
      #define ZEROWORD SINGLEZERO
      #define DFISZERO(df)  ((DFWORD(df, 0)&0x1c0fffff)==0	   \
			  && (DFWORD(df, 0)&0x60000000)!=0x60000000)
    #elif DECPMAX==16
      #define ZEROWORD DOUBLEZERO
      #define DFISZERO(df)  ((DFWORD(df, 1)==0			   \
			  && (DFWORD(df, 0)&0x1c03ffff)==0	   \
			  && (DFWORD(df, 0)&0x60000000)!=0x60000000))
      #define DFISINT(df) ((DFWORD(df, 0)&0x63fc0000)==0x22380000  \
			 ||(DFWORD(df, 0)&0x7bfc0000)==0x6a380000)
      #define DFISUINT01(df) ((DFWORD(df, 0)&0xfbfc0000)==0x22380000)
      #define DFISCCZERO(df) (DFWORD(df, 1)==0			   \
			  && (DFWORD(df, 0)&0x0003ffff)==0)
      #define DFISCC01(df)  ((DFWORD(df, 0)&~0xfffc9124)==0	   \
			  && (DFWORD(df, 1)&~0x49124491)==0)
    #elif DECPMAX==34
      #define ZEROWORD QUADZERO
      #define DFISZERO(df)  ((DFWORD(df, 3)==0			   \
			  &&  DFWORD(df, 2)==0			   \
			  &&  DFWORD(df, 1)==0			   \
			  && (DFWORD(df, 0)&0x1c003fff)==0	   \
			  && (DFWORD(df, 0)&0x60000000)!=0x60000000))
      #define DFISINT(df) ((DFWORD(df, 0)&0x63ffc000)==0x22080000  \
			 ||(DFWORD(df, 0)&0x7bffc000)==0x6a080000)
      #define DFISUINT01(df) ((DFWORD(df, 0)&0xfbffc000)==0x22080000)
      #define DFISCCZERO(df) (DFWORD(df, 3)==0			   \
			  &&  DFWORD(df, 2)==0			   \
			  &&  DFWORD(df, 1)==0			   \
			  && (DFWORD(df, 0)&0x00003fff)==0)

      #define DFISCC01(df)   ((DFWORD(df, 0)&~0xffffc912)==0	   \
			  &&  (DFWORD(df, 1)&~0x44912449)==0	   \
			  &&  (DFWORD(df, 2)&~0x12449124)==0	   \
			  &&  (DFWORD(df, 3)&~0x49124491)==0)
    #endif

    #define CANONDPD(dpd) (((dpd)&0x300)==0 || ((dpd)&0x6e)!=0x6e)
    #define CANONDPDOFF(dpd, k) (((dpd)&(0x300<<(k)))==0	    \
      || ((dpd)&(((uInt)0x6e)<<(k)))!=(((uInt)0x6e)<<(k)))
    #define CANONDPDTWO(hi, lo, k) (((hi)&(0x300>>(32-(k))))==0	    \
      || ((hi)&(0x6e>>(32-(k))))!=(0x6e>>(32-(k)))		    \
      || ((lo)&(((uInt)0x6e)<<(k)))!=(((uInt)0x6e)<<(k)))

    #if DECPMAX==7
      #define ISCOEFFZERO(u) (UINTAT((u)+DECPMAX-4)==0		    \
	&& UINTAT((u)+DECPMAX-7)==0)
    #elif DECPMAX==16
      #define ISCOEFFZERO(u) (UINTAT((u)+DECPMAX-4)==0		    \
	&& (UINTAT((u)+DECPMAX-8)+UINTAT((u)+DECPMAX-12)	    \
	   +UINTAT((u)+DECPMAX-16))==0)
    #elif DECPMAX==34
      #define ISCOEFFZERO(u) (UINTAT((u)+DECPMAX-4)==0		    \
	&& (UINTAT((u)+DECPMAX-8) +UINTAT((u)+DECPMAX-12)	    \
	   +UINTAT((u)+DECPMAX-16)+UINTAT((u)+DECPMAX-20)	    \
	   +UINTAT((u)+DECPMAX-24)+UINTAT((u)+DECPMAX-28)	    \
	   +UINTAT((u)+DECPMAX-32)+USHORTAT((u)+DECPMAX-34))==0)
    #endif

    #define GETECON(df) ((Int)((DFWORD((df), 0)&0x03ffffff)>>(32-6-DECECONL)))
    #define GETWECON(df) ((Int)((DFWWORD((df), 0)&0x03ffffff)>>(32-6-DECWECONL)))
    #define GETEXP(df)	((Int)(DECCOMBEXP[DFWORD((df), 0)>>26]+GETECON(df)))
    #define GETEXPUN(df) ((Int)GETEXP(df)-DECBIAS)
    #define GETMSD(df)	 (DECCOMBMSD[DFWORD((df), 0)>>26])

    #define ECONMASK ((0x03ffffff>>(32-6-DECECONL))<<(32-6-DECECONL))
    #define ECONNANMASK ((0x01ffffff>>(32-6-DECECONL))<<(32-6-DECECONL))


    #define dpd2bcd8(u, dpd) {				 \
      UINTAT(u)=UINTAT(&DPD2BCD8[((dpd)&0x3ff)*4]);}

    #define dpd2bcd83(u, dpd) {				 \
      *(u)=DPD2BCD8[((dpd)&0x3ff)*4];			 \
      *(u+1)=DPD2BCD8[((dpd)&0x3ff)*4+1];		 \
      *(u+2)=DPD2BCD8[((dpd)&0x3ff)*4+2];}



    #if DECPMAX==7
    #define GETCOEFF(df, bcd) {				 \
      uInt sourhi=DFWORD(df, 0);			 \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];		 \
      dpd2bcd8(bcd+1, sourhi>>10);			 \
      dpd2bcd83(bcd+4, sourhi);}
    #define GETWCOEFF(df, bcd) {			 \
      uInt sourhi=DFWWORD(df, 0);			 \
      uInt sourlo=DFWWORD(df, 1);			 \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];		 \
      dpd2bcd8(bcd+1, sourhi>>8);			 \
      dpd2bcd8(bcd+4, (sourhi<<2) | (sourlo>>30));	 \
      dpd2bcd8(bcd+7, sourlo>>20);			 \
      dpd2bcd8(bcd+10, sourlo>>10);			 \
      dpd2bcd83(bcd+13, sourlo);}

    #elif DECPMAX==16
    #define GETCOEFF(df, bcd) {				 \
      uInt sourhi=DFWORD(df, 0);			 \
      uInt sourlo=DFWORD(df, 1);			 \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];		 \
      dpd2bcd8(bcd+1, sourhi>>8);			 \
      dpd2bcd8(bcd+4, (sourhi<<2) | (sourlo>>30));	 \
      dpd2bcd8(bcd+7, sourlo>>20);			 \
      dpd2bcd8(bcd+10, sourlo>>10);			 \
      dpd2bcd83(bcd+13, sourlo);}
    #define GETWCOEFF(df, bcd) {			 \
      uInt sourhi=DFWWORD(df, 0);			 \
      uInt sourmh=DFWWORD(df, 1);			 \
      uInt sourml=DFWWORD(df, 2);			 \
      uInt sourlo=DFWWORD(df, 3);			 \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];		 \
      dpd2bcd8(bcd+1, sourhi>>4);			 \
      dpd2bcd8(bcd+4, ((sourhi)<<6) | (sourmh>>26));	 \
      dpd2bcd8(bcd+7, sourmh>>16);			 \
      dpd2bcd8(bcd+10, sourmh>>6);			 \
      dpd2bcd8(bcd+13, ((sourmh)<<4) | (sourml>>28));	 \
      dpd2bcd8(bcd+16, sourml>>18);			 \
      dpd2bcd8(bcd+19, sourml>>8);			 \
      dpd2bcd8(bcd+22, ((sourml)<<2) | (sourlo>>30));	 \
      dpd2bcd8(bcd+25, sourlo>>20);			 \
      dpd2bcd8(bcd+28, sourlo>>10);			 \
      dpd2bcd83(bcd+31, sourlo);}

    #elif DECPMAX==34
    #define GETCOEFF(df, bcd) {				 \
      uInt sourhi=DFWORD(df, 0);			 \
      uInt sourmh=DFWORD(df, 1);			 \
      uInt sourml=DFWORD(df, 2);			 \
      uInt sourlo=DFWORD(df, 3);			 \
      *(bcd)=(uByte)DECCOMBMSD[sourhi>>26];		 \
      dpd2bcd8(bcd+1, sourhi>>4);			 \
      dpd2bcd8(bcd+4, ((sourhi)<<6) | (sourmh>>26));	 \
      dpd2bcd8(bcd+7, sourmh>>16);			 \
      dpd2bcd8(bcd+10, sourmh>>6);			 \
      dpd2bcd8(bcd+13, ((sourmh)<<4) | (sourml>>28));	 \
      dpd2bcd8(bcd+16, sourml>>18);			 \
      dpd2bcd8(bcd+19, sourml>>8);			 \
      dpd2bcd8(bcd+22, ((sourml)<<2) | (sourlo>>30));	 \
      dpd2bcd8(bcd+25, sourlo>>20);			 \
      dpd2bcd8(bcd+28, sourlo>>10);			 \
      dpd2bcd83(bcd+31, sourlo);}

      #define GETWCOEFF(df, bcd) {??} /* [should never be used]	      */
    #endif


    #define DPD2BIN0 DPD2BIN	     /* for prettier code	      */

    #if DECPMAX==7
    #define GETCOEFFBILL(df, buf) {			      \
      uInt sourhi=DFWORD(df, 0);			      \
      (buf)[0]=DPD2BIN0[sourhi&0x3ff]			      \
	      +DPD2BINK[(sourhi>>10)&0x3ff]		      \
	      +DPD2BINM[DECCOMBMSD[sourhi>>26]];}

    #elif DECPMAX==16
    #define GETCOEFFBILL(df, buf) {			      \
      uInt sourhi, sourlo;				      \
      sourlo=DFWORD(df, 1);				      \
      (buf)[0]=DPD2BIN0[sourlo&0x3ff]			      \
	      +DPD2BINK[(sourlo>>10)&0x3ff]		      \
	      +DPD2BINM[(sourlo>>20)&0x3ff];		      \
      sourhi=DFWORD(df, 0);				      \
      (buf)[1]=DPD2BIN0[((sourhi<<2) | (sourlo>>30))&0x3ff]   \
	      +DPD2BINK[(sourhi>>8)&0x3ff]		      \
	      +DPD2BINM[DECCOMBMSD[sourhi>>26]];}

    #elif DECPMAX==34
    #define GETCOEFFBILL(df, buf) {			      \
      uInt sourhi, sourmh, sourml, sourlo;		      \
      sourlo=DFWORD(df, 3);				      \
      (buf)[0]=DPD2BIN0[sourlo&0x3ff]			      \
	      +DPD2BINK[(sourlo>>10)&0x3ff]		      \
	      +DPD2BINM[(sourlo>>20)&0x3ff];		      \
      sourml=DFWORD(df, 2);				      \
      (buf)[1]=DPD2BIN0[((sourml<<2) | (sourlo>>30))&0x3ff]   \
	      +DPD2BINK[(sourml>>8)&0x3ff]		      \
	      +DPD2BINM[(sourml>>18)&0x3ff];		      \
      sourmh=DFWORD(df, 1);				      \
      (buf)[2]=DPD2BIN0[((sourmh<<4) | (sourml>>28))&0x3ff]   \
	      +DPD2BINK[(sourmh>>6)&0x3ff]		      \
	      +DPD2BINM[(sourmh>>16)&0x3ff];		      \
      sourhi=DFWORD(df, 0);				      \
      (buf)[3]=DPD2BIN0[((sourhi<<6) | (sourmh>>26))&0x3ff]   \
	      +DPD2BINK[(sourhi>>4)&0x3ff]		      \
	      +DPD2BINM[DECCOMBMSD[sourhi>>26]];}

    #endif


    #if DECPMAX==7
    #define GETCOEFFTHOU(df, buf) {			      \
      uInt sourhi=DFWORD(df, 0);			      \
      (buf)[0]=DPD2BIN[sourhi&0x3ff];			      \
      (buf)[1]=DPD2BIN[(sourhi>>10)&0x3ff];		      \
      (buf)[2]=DECCOMBMSD[sourhi>>26];}

    #elif DECPMAX==16
    #define GETCOEFFTHOU(df, buf) {			      \
      uInt sourhi, sourlo;				      \
      sourlo=DFWORD(df, 1);				      \
      (buf)[0]=DPD2BIN[sourlo&0x3ff];			      \
      (buf)[1]=DPD2BIN[(sourlo>>10)&0x3ff];		      \
      (buf)[2]=DPD2BIN[(sourlo>>20)&0x3ff];		      \
      sourhi=DFWORD(df, 0);				      \
      (buf)[3]=DPD2BIN[((sourhi<<2) | (sourlo>>30))&0x3ff];   \
      (buf)[4]=DPD2BIN[(sourhi>>8)&0x3ff];		      \
      (buf)[5]=DECCOMBMSD[sourhi>>26];}

    #elif DECPMAX==34
    #define GETCOEFFTHOU(df, buf) {			      \
      uInt sourhi, sourmh, sourml, sourlo;		      \
      sourlo=DFWORD(df, 3);				      \
      (buf)[0]=DPD2BIN[sourlo&0x3ff];			      \
      (buf)[1]=DPD2BIN[(sourlo>>10)&0x3ff];		      \
      (buf)[2]=DPD2BIN[(sourlo>>20)&0x3ff];		      \
      sourml=DFWORD(df, 2);				      \
      (buf)[3]=DPD2BIN[((sourml<<2) | (sourlo>>30))&0x3ff];   \
      (buf)[4]=DPD2BIN[(sourml>>8)&0x3ff];		      \
      (buf)[5]=DPD2BIN[(sourml>>18)&0x3ff];		      \
      sourmh=DFWORD(df, 1);				      \
      (buf)[6]=DPD2BIN[((sourmh<<4) | (sourml>>28))&0x3ff];   \
      (buf)[7]=DPD2BIN[(sourmh>>6)&0x3ff];		      \
      (buf)[8]=DPD2BIN[(sourmh>>16)&0x3ff];		      \
      sourhi=DFWORD(df, 0);				      \
      (buf)[9]=DPD2BIN[((sourhi<<6) | (sourmh>>26))&0x3ff];   \
      (buf)[10]=DPD2BIN[(sourhi>>4)&0x3ff];		      \
      (buf)[11]=DECCOMBMSD[sourhi>>26];}

    #endif

    #if DECPMAX==7
    #define DFSETNMAX(df)	     \
      {DFWORD(df, 0)=0x77f3fcff;}
    #elif DECPMAX==16
    #define DFSETNMAX(df)	     \
      {DFWORD(df, 0)=0x77fcff3f;     \
       DFWORD(df, 1)=0xcff3fcff;}
    #elif DECPMAX==34
    #define DFSETNMAX(df)	     \
      {DFWORD(df, 0)=0x77ffcff3;     \
       DFWORD(df, 1)=0xfcff3fcf;     \
       DFWORD(df, 2)=0xf3fcff3f;     \
       DFWORD(df, 3)=0xcff3fcff;}
    #endif

  #endif

#endif
