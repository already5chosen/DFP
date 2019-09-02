#include "divide_pow10.h"
#include <string.h>
#include <stdio.h>

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
  enum { DIV1_NMAX = 19, DIV2_NMAX = 27 };
  if (n <= DIV1_NMAX) {
    // 10**n < 2**64. Single-pass division
    uint64_t src2 = src[2];
    uint64_t src1 = src[1];
    uint64_t src0 = src[0];
    if (n==0) {
      result[1] = src1;
      result[0] = src0;
      return 0;
    }
    static const struct {
      uint64_t invF;
      uint64_t halfMulF;
      uint8_t  rshift;
      uint8_t  rshift2;
    } recip_tab0[DIV1_NMAX] = {
     {0xCCCCCCCCCCCCCCCC,                   10ULL / 2,  3,  0   }, //  1
     {0xA3D70A3D70A3D70A,                  100ULL / 2,  6,  0   }, //  2
     {0x83126E978D4FDF3B,                 1000ULL / 2,  9,  9-5 }, //  3
     {0xD1B71758E219652B,                10000ULL / 2, 13, 13-5 }, //  4
     {0xA7C5AC471B478423,               100000ULL / 2, 16, 16-5 }, //  5
     {0x8637BD05AF6C69B5,              1000000ULL / 2, 19, 19-5 }, //  6
     {0xD6BF94D5E57A42BC,             10000000ULL / 2, 23, 23-5 }, //  7
     {0xABCC77118461CEFC,            100000000ULL / 2, 26, 26-5 }, //  8
     {0x89705F4136B4A597,           1000000000ULL / 2, 29, 29-5 }, //  9
     {0xDBE6FECEBDEDD5BE,          10000000000ULL / 2, 33, 33-5 }, // 10
     {0xAFEBFF0BCB24AAFE,         100000000000ULL / 2, 36, 36-5 }, // 11
     {0x8CBCCC096F5088CB,        1000000000000ULL / 2, 39, 39-5 }, // 12
     {0xE12E13424BB40E13,       10000000000000ULL / 2, 43, 43-5 }, // 13
     {0xB424DC35095CD80F,      100000000000000ULL / 2, 46, 46-5 }, // 14
     {0x901D7CF73AB0ACD9,     1000000000000000ULL / 2, 49, 49-5 }, // 15
     {0xE69594BEC44DE15B,    10000000000000000ULL / 2, 53, 53-5 }, // 16
     {0xB877AA3236A4B449,   100000000000000000ULL / 2, 56, 56-5 }, // 17
     {0x9392EE8E921D5D07,  1000000000000000000ULL / 2, 59, 59-5 }, // 18
     {0xEC1E4A7DB69561A5, 10000000000000000000ULL / 2, 63, 63-7 }, // 19
    };
    uint64_t invF = recip_tab0[n-1].invF;
    uint64_t halfMulF = recip_tab0[n-1].halfMulF;
    uint64_t mulF = halfMulF+halfMulF;
    unsigned rshift = recip_tab0[n-1].rshift;
    unsigned rshift2 = recip_tab0[n-1].rshift2;
    const uint64_t MSK56 = (uint64_t)-1 >> 8;

    uint64_t d1 = (src1 << 8) | (src0 >> 56);
    uint64_t d0 = src0 & MSK56;
    uint64_t r1, r0;
    if (n <= 4) {
      if (n <= 2) {
        r1 = __umulh(d1, invF) >> rshift;
      } else {
        r1 = __umulh(src1, invF) >> (rshift-8);
      }
    } else {
      // src2 contains at most rshift+1-16 significant bits
      // It can be shifted to the left by 64-(rshift+1-16)=79-rshift bits
      uint64_t dh1 = (src2 << (79-rshift)) | (src1 >> (rshift-15));
      r1 = __umulh(dh1, invF) >> 7;
    }
    uint64_t rem0 = d1 - r1*mulF;
    uint64_t d0h = (rem0 << (56-rshift2)) | (d0 >> rshift2);
    d0 |= (rem0 << 56);
    r0 = __umulh(d0h, invF) >> (rshift-rshift2);
    uint64_t rem = d0 - r0*mulF;
    // if (n==19)printf("rem0=%20llu rem=%20llu\n", rem0, rem);
    // while (rem >= mulF) {
    if (rem >= mulF) {
      rem -= mulF;
      r0  += 1;
    }
    r1 += (r0 >> 56); r0 &= MSK56;
    result[0] = (r1 << 56) | r0;
    result[1] = r1 >> 8;
    return rem < halfMulF ? (rem != 0) : (rem != halfMulF) + 2;
  }

  // n > 19
  // Divide source by 2**(n-1)
  // Result of division contains at most 191 bit, so it fits easily in 3 64-bit word
  uint64_t src2 = (src[3] << (65-n)) | (src[2] >> (n-1));
  uint64_t src1 = (src[2] << (65-n)) | (src[1] >> (n-1));
  uint64_t src0 = (src[1] << (65-n)) | (src[0] >> (n-1));
  uint64_t steaky = src[0] << (65-n); // MS bits = src[] % 2**(n-1)
  if (n <= DIV2_NMAX) {
    // 5**n < 2**64
    static const struct {
      uint64_t invF;
      uint64_t mulF;
      uint8_t  rshift;
    } recip_tab1[DIV2_NMAX-DIV1_NMAX] = {
     {0xBCE5086492111AEA,       95367431640625, 46 }, // 20
     {0x971DA05074DA7BEE,      476837158203125, 48 }, // 21
     {0xF1C90080BAF72CB1,     2384185791015625, 51 }, // 22
     {0xC16D9A0095928A27,    11920928955078125, 53 }, // 23
     {0x9ABE14CD44753B52,    59604644775390625, 55 }, // 24
     {0xF79687AED3EEC551,   298023223876953125, 58 }, // 25
     {0xC612062576589DDA,  1490116119384765625, 60 }, // 26
     {0x9E74D1B791E07E48,  7450580596923828125, 62 }, // 27
    };
    unsigned rshift = recip_tab1[n-1-DIV1_NMAX].rshift;
    uint64_t invF = recip_tab1[n-1-DIV1_NMAX].invF;
    uint64_t mulF = recip_tab1[n-1-DIV1_NMAX].mulF;
    const uint64_t MSK56 = (uint64_t)-1 >> 8;
    // src2:src1:src0 contains at most 112+1+rshift+1 significant bits.
    // It means that src2  contains at most 112+1+rshift+1-128=rshift-14 significant bits
    // It can be shifted to the left by 64-(rshift-14)=78-rshift bits
    uint64_t dh1 = (src2 << (78-rshift)) | (src1 >> (rshift-14));
    uint64_t r1 = __umulh(dh1, invF) >> 6;
    uint64_t d1 = (src1 << 8) | (src0 >> 56);
    uint64_t d0 = src0 & MSK56;
    uint64_t rem0 = d1 - r1*mulF;
    uint64_t d0h = (rem0 << (62-rshift)) | (d0 >> (rshift-6));
    d0 |= (rem0 << 56);
    uint64_t r0 = __umulh(d0h, invF) >> 6;
    uint64_t rem = d0 - r0*mulF;
    // while (rem >= mulF) {
    if (rem >= mulF) {
      rem -= mulF;
      r0  += 1;
    }
    steaky |= rem;
    r1 += (r0 >> 56); r0 &= MSK56;
    result[0] = (r1 << 55) | (r0 >> 1);
    result[1] = r1 >> 9;
    return (r0 & 1) * 2 | (steaky != 0);
  }

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
