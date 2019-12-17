#include "divide_pow10.h"
#include <string.h>

#if REPORT_UNDERFLOWS
uint8_t gl_underflow;
#endif

// DivideDecimal68ByPowerOf10 - Divide unsigned integer number by power of ten
//
// Arguments:
// result - result of division, 2 64-bit words, range [0:10**34-1], Little Endian
// src    - source (dividend), 4 64-bit words, range [0:10**68-1], Little Endian
// n      - decimal exponent of the divisor, i.e. divisor=10**n, range 0 to 34
// Return value:  0 when remainder of division ==0
//                1 when remainder of division >0 and < divisor/2,
//                2 when remainder == divisor/2,
//                3 when remainder > divisor/2
//
// Comments:
// 1. It works only on byte-addressable Little Endian machines
// 2. When src >= 10**n * 2**112 the results are incorrect, but the call is still legal
//    in a sense that it causes no memory corruptions, traps or any other undefined actions
int DivideDecimal68ByPowerOf10(uint64_t result[2], const uint64_t src[4], unsigned n)
{
  #if REPORT_UNDERFLOWS
  gl_underflow = 0;
  #endif
  enum { NMAX = 34 };

  if (n-1 > NMAX-1) {
    result[0] = src[0];
    result[1] = src[1];
    return 0;
  }

  static const struct {
    uint8_t  src_offs;  // maximal number of bits in src[] rounded up to the whole octet - 128 / 8 clipped to non-negative
                        // invF = 2**(src_offs*8+160) / mulF
    uint8_t  src_offsLL;
    uint8_t  rem_offs;  // (n-1)/8
    uint8_t  shift_LL;
    uint32_t invF_l;    // invF[31:0]
    uint64_t invF_m;    // invF[95:32]
    uint64_t invF_h;    // invF[160:96]
    uint64_t mulF_l;    // (10**n / 2 / 256**rem_offs) % 2**64
    uint32_t steaky_msk;
  } recip_tab[NMAX] = {
 { 0, 24, 0,  0, 0xc3c598d9, 0x33333333333335db, 0x3333333333333333, 0x0000000000000005, 0x00000000 }, //  1
 { 0, 24, 0,  0, 0xecf73956, 0x51eb851eb851eb8b, 0x051eb851eb851eb8, 0x0000000000000032, 0x00000000 }, //  2
 { 1,  0, 0, 24, 0xe4fb256b, 0x645a1cac083126fa, 0x83126e978d4fdf3b, 0x00000000000001f4, 0x00000000 }, //  3
 { 0, 24, 0,  0, 0x4ab8af47, 0x52bd3c36113404ea, 0x000d1b71758e2196, 0x0000000000001388, 0x00000000 }, //  4
 { 1,  0, 0, 24, 0xa80de8e4, 0x461f9f01b866e43a, 0x014f8b588e368f08, 0x000000000000c350, 0x00000000 }, //  5
 { 1,  0, 0, 24, 0x43f71d62, 0x6d698fe69270b06c, 0x00218def416bdb1a, 0x000000000007a120, 0x00000000 }, //  6
 { 2,  0, 0, 16, 0x32356e7d, 0xf0f4ca41d811a46d, 0x035afe535795e90a, 0x00000000004c4b40, 0x00000000 }, //  7
 { 2,  0, 0, 16, 0x1e9eae1a, 0x7e7ee106959b5d3e, 0x0055e63b88c230e7, 0x0000000002faf080, 0x00000000 }, //  8
 { 2,  0, 1, 16, 0x030fdd89, 0x59731680a88f8953, 0x00089705f4136b4a, 0x00000000001dcd65, 0x000000ff }, //  9
 { 4,  0, 1,  0, 0x2fbf3807, 0xb573440e5a884d1b, 0xdbe6fecebdedd5be, 0x00000000012a05f2, 0x000000ff }, // 10
 { 4,  0, 1,  0, 0x1e5fe796, 0xdef1ed34a2a73ae9, 0x15fd7fe17964955f, 0x000000000ba43b74, 0x000000ff }, // 11
 { 4,  0, 1,  0, 0x4fd663ea, 0x2fe4fe1edd10b917, 0x0232f33025bd4223, 0x00000000746a5288, 0x000000ff }, // 12
 { 4,  0, 1,  0, 0xee623d30, 0x84ca19697c81ac1b, 0x00384b84d092ed03, 0x000000048c273950, 0x000000ff }, // 13
 { 4,  0, 1,  0, 0x64a36c84, 0xc07a9c24260cf79c, 0x0005a126e1a84ae6, 0x0000002d79883d20, 0x000000ff }, // 14
 { 5,  1, 1,  0, 0x1057a6e3, 0xd90f9d37014bf60a, 0x00901d7cf73ab0ac, 0x000001c6bf526340, 0x000000ff }, // 15
 { 5,  1, 1,  0, 0x9b3bf716, 0x15b4c2ebe687989a, 0x000e69594bec44de, 0x000011c37937e080, 0x000000ff }, // 16
 { 6,  2, 2,  0, 0x85ff1be0, 0x92137dfd73f5a90f, 0x0170ef54646d4968, 0x000000b1a2bc2ec5, 0x0000ffff }, // 17
 { 6,  2, 2,  0, 0xf3ccb5fc, 0x41cebfcc8b9890e7, 0x0024e4bba3a48757, 0x000006f05b59d3b2, 0x0000ffff }, // 18
 { 7,  3, 2,  0, 0x94789948, 0x94acc7a78f41b0cb, 0x03b07929f6da5586, 0x00004563918244f4, 0x0000ffff }, // 19
 { 7,  3, 2,  0, 0xc20c0f54, 0x75447a5d8e535e7a, 0x005e72843249088d, 0x0002b5e3af16b188, 0x0000ffff }, // 20
 { 7,  3, 2,  0, 0xe03467ee, 0xbeed3f6fc16ebca5, 0x000971da05074da7, 0x001b1ae4d6e2ef50, 0x0000ffff }, // 21
 { 8,  4, 2,  0, 0x3870cb14, 0xb15324c68b12dd63, 0x00f1c90080baf72c, 0x010f0cf064dd5920, 0x0000ffff }, // 22
 { 8,  4, 2,  0, 0x5271ade8, 0x44eeb6e0a781e2f0, 0x00182db34012b251, 0x0a968163f0a57b40, 0x0000ffff }, // 23
 { 9,  5, 2,  0, 0x0b5e30d8, 0x4b1249aa59c9e4d5, 0x026af8533511d4ed, 0x69e10de76676d080, 0x0000ffff }, // 24
 { 9,  5, 3,  0, 0x812304e2, 0x544ea0f76f60fd48, 0x003de5a1ebb4fbb1, 0x0422ca8b0a00a425, 0x00ffffff }, // 25
 { 9,  5, 3,  0, 0x0ce9e6e3, 0xeed4a9b257f01954, 0x00063090312bb2c4, 0x295be96e64066972, 0x00ffffff }, // 26
 {10,  6, 3,  0, 0x7dca49f1, 0x48775ea264cf5534, 0x009e74d1b791e07e, 0x9d971e4fe8401e74, 0x00ffffff }, // 27
 {10,  6, 3,  0, 0x3fc76dcb, 0xa0d8bca9d6e18885, 0x000fd87b5f28300c, 0x27e72f1f12813088, 0x00ffffff }, // 28
 {11,  7, 3,  0, 0x60be2df0, 0x7c12ddc8b0274086, 0x0195a5efea6b3476, 0x8f07d736b90be550, 0x00ffffff }, // 29
 {11,  7, 3,  0, 0xa34637cb, 0x0c684960de6a5340, 0x00289097fdd7853f, 0x964e68233a76f520, 0x00ffffff }, // 30
 {11,  7, 3,  0, 0x76ba38c7, 0xe7a407567ca43b86, 0x00040e7599625a1f, 0xdf10116048a59340, 0x00ffffff }, // 31
 {12,  8, 3,  0, 0x7905ad8d, 0x5d33ef0c76d2c0a5, 0x0067d88f56a29cca, 0xb6a0adc2d677c080, 0x00ffffff }, // 32
 {12,  8, 4,  0, 0x58e6f7c1, 0xd61ecb1ad8aeacdd, 0x000a6274bbdd0fad, 0xe32246c99c60ad85, 0xffffffff }, // 33
 {13,  9, 4,  0, 0x7d7f2cee, 0x697ab5e277de1622, 0x0109d8792fb4c495, 0xdf56c3e01bc6c732, 0xffffffff }, // 34
};

  // Fetch upper 128 bits of the src[]
  // Attention this code works only on byte-addressable Little Endian machines!
  const unsigned src_offs = recip_tab[n-1].src_offs;
  uint64_t srcH, srcL;
  memcpy(&srcH, (char*)src + src_offs + 8, sizeof(srcH));
  memcpy(&srcL, (char*)src + src_offs + 0, sizeof(srcL));

#ifndef _MSC_VER
  // Multiplication by reciprocal
  typedef unsigned __int128 uintex_t;
  const uint64_t invF_m = recip_tab[n-1].invF_m;
  const uint64_t invF_h = recip_tab[n-1].invF_h;
  uintex_t rxxL = (uintex_t)srcH * invF_m;
  rxxL         += (uint64_t)((uintex_t)srcL * invF_m >> 64);
  rxxL         += (uintex_t)srcL * invF_h;
  uint64_t rxxLL = (srcH >> 32) * (uint64_t)recip_tab[n-1].invF_l;
  uint32_t srcLL;
  memcpy(&srcLL, (char*)src + recip_tab[n-1].src_offsLL, sizeof(srcLL));
  srcLL <<= recip_tab[n-1].shift_LL;
  rxxLL += srcLL * (invF_h >> 32); // no overflow here
  rxxL += rxxLL;
  uintex_t rx = (uintex_t)srcH * invF_h + (uint64_t)(rxxL >> 64);

  // calculate LS word of reminder
  const uint64_t mulF_l = recip_tab[n-1].mulF_l;
  uint64_t src0;
  memcpy(&src0, (char*)src + recip_tab[n-1].rem_offs, sizeof(src0));
  uint64_t rem0 = src0 - (uint64_t)rx * mulF_l;  // remainder in rem0

  const uint64_t rxxL_thr = (uint64_t)(-1) << 36;
  if ((uint64_t)rxxL >= rxxL_thr) {
    // Fractional part is close to 1, check for underflow
    if (rem0 >= mulF_l) {
      #if REPORT_UNDERFLOWS
      gl_underflow = 1;
      #endif
      rem0 -= mulF_l;
      rx += 1;
    }
  }
  uint64_t r1 = rx >> 64;
  uint64_t r0 = rx;
#else
  // Multiplication by reciprocal
  const uint64_t invF_m = recip_tab[n-1].invF_m;
  uint64_t rF, r0, r1, rxLH0, rxLH1, rxHH0;
  rF   = _umul128(srcH, invF_m, &r0);
  uint8_t carry;
  carry = _addcarry_u64(0,     rF, __umulh(srcL, invF_m), &rF);
  carry = _addcarry_u64(carry, r0, 0,                     &r0);
  const uint64_t invF_h = recip_tab[n-1].invF_h;
  rxLH0 = _umul128(srcL, invF_h, &rxLH1);
  carry = _addcarry_u64(0,     rF, rxLH0, &rF);
  carry = _addcarry_u64(carry, r0, rxLH1, &r0);
  uint64_t rxxLL = (srcH >> 32) * (uint64_t)recip_tab[n-1].invF_l;
  uint32_t srcLL;
  memcpy(&srcLL, (char*)src + recip_tab[n-1].src_offsLL, sizeof(srcLL));
  srcLL <<= recip_tab[n-1].shift_LL;
  rxxLL += srcLL * (invF_h >> 32); // no overflow here
  carry = _addcarry_u64(0,     rF, rxxLL, &rF);
  carry = _addcarry_u64(carry, r0, 0,     &r0);
  rxHH0 = _umul128(srcH, invF_h, &r1);
  carry = _addcarry_u64(0,     r0, rxHH0, &r0);
  carry = _addcarry_u64(carry, r1, 0,     &r1);
  // Integer part of result in r1:r0. Fractional part in rF

  // calculate LS word of reminder
  const uint64_t mulF_l = recip_tab[n-1].mulF_l;
  uint64_t src0;
  memcpy(&src0, (char*)src + recip_tab[n-1].rem_offs, sizeof(src0));
  uint64_t rem0 = src0 - (uint64_t)r0 * mulF_l;  // remainder in rem0

  const uint64_t rF_thr = (uint64_t)(-1) << 36;
  if (rF >= rF_thr) {
    // Fractional part is close to 1, check for underflow
    if (rem0 >= mulF_l) {
      #if REPORT_UNDERFLOWS
      gl_underflow = 1;
      #endif
      rem0 -= mulF_l;
      carry = _addcarry_u64(0,     r0, 1, &r0);
      carry = _addcarry_u64(carry, r1, 0, &r1);
    }
  }
#endif

  result[0] = (r1 << 63) | (r0 >> 1);
  result[1] = r1 >> 1;
  uint64_t steaky = rem0 | (src[0] & recip_tab[n-1].steaky_msk);
  return ((int)r0 & 1) *2 + (steaky != 0);
}
