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
  enum { DIV1_NMAX = 19, DIV3_NMAX = 27, NMAX = 34, DIV2_N = 8 };
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

  if (n <= DIV3_NMAX) {
    // 5**n < 2**64
    static const struct {
      uint64_t invF;
      uint64_t mulF;
      uint8_t  rshift;
    } recip_tab1[DIV3_NMAX-DIV1_NMAX] = {
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

  if (n > NMAX)
    return 0; // should not happen

  // n < DIV3_NMAX  <= NMAX
  // 5**n > 2**64
  // divide by 5**n
  static const struct {
    uint64_t invF;
    uint64_t mulF_l;
    uint32_t mulF_h;
    uint8_t  rshift;
  } recip_tab2[NMAX-DIV3_NMAX] = {
   {0xFD87B5F28300CA0D, 0x04FCE5E3E2502611, 0x0002, 65-64 }, // 28
   {0xCAD2F7F5359A3B3E, 0x18F07D736B90BE55, 0x000A, 67-64 }, // 29
   {0xA2425FF75E14FC31, 0x7CB2734119D3B7A9, 0x0032, 69-64 }, // 30
   {0x81CEB32C4B43FCF4, 0x6F7C40458122964D, 0x00FC, 71-64 }, // 31
   {0xCFB11EAD453994BA, 0x2D6D415B85ACEF81, 0x04EE, 74-64 }, // 32
   {0xA6274BBDD0FADD61, 0xE32246C99C60AD85, 0x18A6, 76-64 }, // 33
   {0x84EC3C97DA624AB4, 0x6FAB61F00DE36399, 0x7B42, 78-64 }, // 34
  };
  typedef unsigned __int128 uintex_t;
  const unsigned rshift = recip_tab2[n-DIV3_NMAX-1].rshift;
  const uint64_t invF   = recip_tab2[n-DIV3_NMAX-1].invF;   // 2**(rshift+128)/5**n
  const uint64_t mulF_h = recip_tab2[n-DIV3_NMAX-1].mulF_h; // 5**n / 2^64
  const uint64_t mulF_l = recip_tab2[n-DIV3_NMAX-1].mulF_l; // 5**n % 2^64
  const uintex_t mulF   = ((uintex_t)mulF_h << 64) | mulF_l;// 5**n
  const uint64_t MSK32  = (uint64_t)-1 >> (64-32);

  uint64_t dh = src2;
  uint64_t dl = src1;
  uint64_t r2 = __umulh(dh, invF) >> rshift; // r2 aligned at bit 64
  uintex_t d = ((uintex_t)dh << 64) | dl;
  d -= (uintex_t)r2 * mulF_l;
  d -= (uintex_t)(r2 * mulF_h) << 64;

  dh = d >> 32;
  dl = ((uint64_t)d << 32) | (src0 >> 32);
  uint64_t r1 = __umulh(dh, invF) >> rshift; // r1 aligned at bit 32
  d = ((uintex_t)dh << 64) | dl;
  d -= (uintex_t)r1 * mulF_l;
  d -= (uintex_t)(r1 * mulF_h) << 64;

  dh = d >> 32;
  dl = ((uint64_t)d << 32) | (src0 & MSK32);
  uint64_t r0 = __umulh(dh, invF) >> rshift; // r0 aligned at bit 32
  d = ((uintex_t)dh << 64) | dl;
  d -= (uintex_t)r0 * mulF_l;
  d -= (uintex_t)(r0 * mulF_h) << 64;

  if (d >= mulF) {
    d -= mulF;
    r0 += 1;
  }

  steaky |= (uint64_t)d;
  steaky |= (uint64_t)(d >> 64);

  uintex_t rr = ((uintex_t)r2 << 64) | r0;
  rr += (uintex_t)r1 << 32;
  r0 = (uint64_t)rr;
  r1 = (uint64_t)(rr >> 64);
  result[1] = r1 >> 1;
  result[0] = (r1 << 63) | (r0 >> 1);
  return (r0 & 1) * 2 | (steaky != 0);
}
