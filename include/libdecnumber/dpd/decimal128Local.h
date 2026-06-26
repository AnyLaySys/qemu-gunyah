
#if !defined(DECIMAL128LOCAL)


#define decimal128SetSign(d,b) \
  { (d)->bytes[WORDS_BIGENDIAN ? 0 : 15] |= ((unsigned) (b) << 7); }

#define decimal128ClearSign(d) \
  { (d)->bytes[WORDS_BIGENDIAN ? 0 : 15] &= ~0x80; }

#define decimal128FlipSign(d) \
  { (d)->bytes[WORDS_BIGENDIAN ? 0 : 15] ^= 0x80; }

#endif
