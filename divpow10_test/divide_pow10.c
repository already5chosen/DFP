#include "divide_pow10.h"
#include <string.h>

#ifndef _MSC_VER
static inline uint64_t __umulh(uint64_t a, uint64_t b) {
  return (uint64_t)(((unsigned __int128)a * b) >> 64);
}
#endif

// DivideUint224ByPowerOf10 - Divide unsigned integer number by power of ten
//
// Arguments:
// result - result of division, 2 64-bit words, up to 112 significant bits, Little Endian
// src    - source (dividend), 4 64-bit words, up to 224 significant bits, Little Endian
// n      - decimal exponent of the divisor, i.e. divisor=10**n, range 0 to 34
// Return value:  0 when remainder of division ==0
//                1 when remainder of division >0 and < divisor/2,
//                2 when remainder == divisor/2,
//                3 when remainder > divisor/2
//
// Comments:
// 1. It works only on Little Endian machines with sizeof(uint32_t)*2=sizeof(uint64_t)
// 2. When src >= 10**n * 2**112 the results are incorrect, but the call is still legal
//    in a sense that it causes no memory corruptions, traps or any other undefined actions
int DivideUint224ByPowerOf10(uint64_t result[2], const uint64_t src[4], unsigned n)
{
  if (n <= 1) {
    uint64_t src1 = src[1];
    uint64_t src0 = src[0];
    uint64_t rem = 0;
    if (n != 0) {
      uint64_t d = src1;
      src1 = d / 10;
      rem = d - src1*10;
      d = (rem << 32) | (src0 >> 32);
      uint64_t src0h = d / 10;
      rem = d - src0h*10;
      d = (rem << 32) | (src0 & (uint32_t)(-1));
      uint64_t src0l = d / 10;
      rem = d - src0l*10;
      src0 = (src0h << 32) | src0l;
    }
    result[0] = src0;
    result[1] = src1;
    return rem < 5 ? rem != 0 : (rem != 5) + 2;
  }

  // n > 1
  // Divide source by 2**(n-1)
  // Result of division contains at most 191 bit, so it fits easily in 3 64-bit word
  uint64_t src2 = (src[3] << (65-n)) | (src[2] >> (n-1));
  uint64_t src1 = (src[2] << (65-n)) | (src[1] >> (n-1));
  uint64_t src0 = (src[1] << (65-n)) | (src[0] >> (n-1));
  uint64_t steaky = src[0] << (65-n); // MS bits = src[] % 2**(n-1)

  enum { N0 = 13 };
  if (n >= N0) { // divide by 5**13
    enum { DV = 1220703125ul };
    do {
      uint64_t d = src2, rem, h, l;
      src2 = d / DV; rem = d - src2*DV;

      d = (rem << 32) | (src1 >> 32);
      h = d / DV; rem = d - h*DV;
      d = (rem << 32) | (src1 & (uint32_t)(-1));
      l = d / DV; rem = d - l*DV;
      src1 = (h << 32) | l;

      d = (rem << 32) | (src0 >> 32);
      h = d / DV; rem = d - h*DV;
      d = (rem << 32) | (src0 & (uint32_t)(-1));
      l = d / DV; rem = d - l*DV;
      src0 = (h << 32) | l;

      steaky |= rem;
      n -= N0;
    } while (n >= N0);

    if (n == 0) { // done
      result[0] = (src1 << 63) | (src0 >> 1);
      result[1] = src1 >> 1;
      return (src0 & 1) * 2 | (steaky != 0);
    }
    // not done yet
  }

  // n < 13 => src < 5**12 * 2 * 2**112 < 2**141
  static const struct {
    uint64_t invF;
    uint32_t mulF;
    uint32_t rshift;
  } recip_tab[N0-1] = {
   // reciprocals is not necessarily correct for a full 64-bit range
   // but they are correct for the range that we care about, i.e. [0:(5**12 * 2**32)-1]
   {0xCCCCCCCCCCCCCCCD,         5,  2 }, //  1
   {0xA3D70A3D70A3D70B,        25,  4 }, //  2
   {0x83126E978D4FDF3C,       125,  6 }, //  3
   {0xD1B71758E219652C,       625,  9 }, //  4
   {0xA7C5AC471B478424,      3125, 11 }, //  5
   {0x8637BD05AF6C69B6,     15625, 13 }, //  6
   {0xD6BF94D5E57A42BD,     78125, 16 }, //  7
   {0xABCC77118461CEFD,    390625, 18 }, //  8
   {0x89705F4136B4A598,   1953125, 20 }, //  9
   {0xDBE6FECEBDEDD5BF,   9765625, 23 }, // 10
   {0xAFEBFF0BCB24AAFF,  48828125, 25 }, // 11
   {0x8CBCCC096F5088CC, 244140625, 27 }, // 12
  };
  uint64_t invF = recip_tab[n-1].invF;
  uint64_t mulF = recip_tab[n-1].mulF;
  unsigned rshift = recip_tab[n-1].rshift;

  uint64_t d, rem;
  d = (src2 << 32) | (src1 >> 32);
  uint64_t r3 = __umulh(d, invF) >> rshift; rem = d - r3*mulF;
  d = (rem << 32) | (src1 & (uint32_t)(-1));
  uint64_t r2 = __umulh(d, invF) >> rshift; rem = d - r2*mulF;
  d = (rem << 32) | (src0 >> 32);
  uint64_t r1 = __umulh(d, invF) >> rshift; rem = d - r1*mulF;
  d = (rem << 32) | (src0 & (uint32_t)(-1));
  uint64_t r0 = __umulh(d, invF) >> rshift; rem = d - r0*mulF;
  steaky |= rem;

  result[0] = (r0 >> 1) | (r1 << 31) | (r2 << 63);
  result[1] = (r2 >> 1) | (r3 << 31);

  return (r0 & 1) * 2 | (steaky != 0);
}
