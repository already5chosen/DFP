#include "divide_pow10.h"
#include <string.h>

#ifndef _MSC_VER
static inline uint64_t __umulh(uint64_t a, uint64_t b) {
  return (uint64_t)(((unsigned __int128)a * b) >> 64);
}
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
  enum { NMAX = 34 };

  if (n-1 > NMAX-1) {
    result[0] = src[0];
    result[1] = src[1];
    return 0;
  }

  static const uint8_t nb_tab[NMAX] = { // maximal number of bits in src[] rounded up to the whole octet - 128
   (128 - 128)/8, //  1   0
   (128 - 128)/8, //  2   0
   (128 - 128)/8, //  3   0
   (128 - 128)/8, //  4   0
   (136 - 128)/8, //  5   1
   (136 - 128)/8, //  6   1
   (144 - 128)/8, //  7   2
   (144 - 128)/8, //  8   2
   (144 - 128)/8, //  9   2
   (152 - 128)/8, // 10   3
   (152 - 128)/8, // 11   3
   (160 - 128)/8, // 12   4
   (160 - 128)/8, // 13   4
   (160 - 128)/8, // 14   4
   (168 - 128)/8, // 15   5
   (168 - 128)/8, // 16   5
   (176 - 128)/8, // 17   6
   (176 - 128)/8, // 18   6
   (184 - 128)/8, // 19   7
   (184 - 128)/8, // 20   7
   (184 - 128)/8, // 21   7
   (192 - 128)/8, // 22   8
   (192 - 128)/8, // 23   8
   (200 - 128)/8, // 24   9
   (200 - 128)/8, // 25   9
   (200 - 128)/8, // 26   9
   (208 - 128)/8, // 27  10
   (208 - 128)/8, // 28  10
   (216 - 128)/8, // 29  11
   (216 - 128)/8, // 30  11
   (216 - 128)/8, // 31  11
   (224 - 128)/8, // 32  12
   (224 - 128)/8, // 33  12
   (232 - 128)/8, // 34  13
  };
  // Fetch upper 128 bits of the src[]
  // Attention this code works only on byte-addressable Little Endian machines!
  unsigned srcH_offs = nb_tab[n-1];
  uint64_t srcH, srcL;
  memcpy(&srcH, (char*)src + srcH_offs + 8, sizeof(srcH));
  memcpy(&srcL, (char*)src + srcH_offs + 0, sizeof(srcL));

  static const struct {
    uint64_t mulF_l;  // 10**n / 2
    uint64_t mulF_h;
    uint64_t invF_l;  // 2**(srcH_offs*8+130) / mulF
    uint64_t invF_h;
  } recip_tab[NMAX] = {
   {0x0000000000000005, 0x0000000000000000, 0xCCCCCCCCCCCCCCCC, 0xCCCCCCCCCCCCCCCC }, //  1
   {0x0000000000000032, 0x0000000000000000, 0x47AE147AE147AE14, 0x147AE147AE147AE1 }, //  2
   {0x00000000000001F4, 0x0000000000000000, 0xED916872B020C49B, 0x020C49BA5E353F7C }, //  3
   {0x0000000000001388, 0x0000000000000000, 0x4AF4F0D844D013A9, 0x00346DC5D6388659 }, //  4
   {0x000000000000C350, 0x0000000000000000, 0x187E7C06E19B90EA, 0x053E2D6238DA3C21 }, //  5
   {0x000000000007A120, 0x0000000000000000, 0xB5A63F9A49C2C1B1, 0x008637BD05AF6C69 }, //  6
   {0x00000000004C4B40, 0x0000000000000000, 0xC3D32907604691B4, 0x0D6BF94D5E57A42B }, //  7
   {0x0000000002FAF080, 0x0000000000000000, 0xF9FB841A566D74F8, 0x015798EE2308C39D }, //  8
   {0x000000001DCD6500, 0x0000000000000000, 0x65CC5A02A23E254C, 0x00225C17D04DAD29 }, //  9
   {0x000000012A05F200, 0x0000000000000000, 0xFAD5CD10396A2134, 0x036F9BFB3AF7B756 }, // 10
   {0x0000000BA43B7400, 0x0000000000000000, 0x7F7BC7B4D28A9CEB, 0x0057F5FF85E59255 }, // 11
   {0x000000746A528800, 0x0000000000000000, 0xBF93F87B7442E45D, 0x08CBCCC096F5088C }, // 12
   {0x0000048C27395000, 0x0000000000000000, 0x132865A5F206B06F, 0x00E12E13424BB40E }, // 13
   {0x00002D79883D2000, 0x0000000000000000, 0x01EA70909833DE71, 0x0016849B86A12B9B }, // 14
   {0x0001C6BF52634000, 0x0000000000000000, 0x643E74DC052FD828, 0x024075F3DCEAC2B3 }, // 15
   {0x0011C37937E08000, 0x0000000000000000, 0x56D30BAF9A1E626A, 0x0039A5652FB11378 }, // 16
   {0x00B1A2BC2EC50000, 0x0000000000000000, 0x484DF7F5CFD6A43E, 0x05C3BD5191B525A2 }, // 17
   {0x06F05B59D3B20000, 0x0000000000000000, 0x073AFF322E62439F, 0x009392EE8E921D5D }, // 18
   {0x4563918244F40000, 0x0000000000000000, 0x52B31E9E3D06C32E, 0x0EC1E4A7DB69561A }, // 19
   {0xB5E3AF16B1880000, 0x0000000000000002, 0xD511E976394D79EB, 0x0179CA10C9242235 }, // 20
   {0x1AE4D6E2EF500000, 0x000000000000001B, 0xFBB4FDBF05BAF297, 0x0025C768141D369E }, // 21
   {0x0CF064DD59200000, 0x000000000000010F, 0xC54C931A2C4B758C, 0x03C7240202EBDCB2 }, // 22
   {0x8163F0A57B400000, 0x0000000000000A96, 0x13BADB829E078BC1, 0x0060B6CD004AC945 }, // 23
   {0x0DE76676D0800000, 0x00000000000069E1, 0x2C4926A967279354, 0x09ABE14CD44753B5 }, // 24
   {0x8B0A00A425000000, 0x00000000000422CA, 0x513A83DDBD83F522, 0x00F79687AED3EEC5 }, // 25
   {0x6E64066972000000, 0x0000000000295BE9, 0xBB52A6C95FC06550, 0x0018C240C4AECB13 }, // 26
   {0x4FE8401E74000000, 0x00000000019D971E, 0x21DD7A89933D54D1, 0x0279D346DE4781F9 }, // 27
   {0x1F12813088000000, 0x000000001027E72F, 0x8362F2A75B862214, 0x003F61ED7CA0C032 }, // 28
   {0x36B90BE550000000, 0x00000000A18F07D7, 0xF04B7722C09D0219, 0x065697BFA9ACD1D9 }, // 29
   {0x233A76F520000000, 0x000000064F964E68, 0x31A1258379A94D02, 0x00A2425FF75E14FC }, // 30
   {0x6048A59340000000, 0x0000003F1BDF1011, 0x9E901D59F290EE19, 0x001039D66589687F }, // 31
   {0xC2D677C080000000, 0x0000027716B6A0AD, 0x74CFBC31DB4B0295, 0x019F623D5A8A7329 }, // 32
   {0x9C60AD8500000000, 0x000018A6E32246C9, 0x587B2C6B62BAB375, 0x002989D2EF743EB7 }, // 33
   {0x1BC6C73200000000, 0x0000F684DF56C3E0, 0xA5EAD789DF785889, 0x042761E4BED31255 }, // 34
  };
  const uint64_t invF_h = recip_tab[n-1].invF_h;
  const uint64_t invF_l = recip_tab[n-1].invF_l;
  const uint64_t mulF_h = recip_tab[n-1].mulF_h;
  const uint64_t mulF_l = recip_tab[n-1].mulF_l;
  uint64_t src1 = src[1];
  uint64_t src0 = src[0];

#ifndef _MSC_VER
  typedef unsigned __int128 uintex_t;
  uintex_t rx = (uintex_t)srcH * invF_h;
  rx += __umulh(srcH, invF_l);
  rx += __umulh(srcL, invF_h);
  rx >>= 2;
  uint64_t r1 = rx >> 64;
  uint64_t r0 = rx;
  src1 -= r1*mulF_l;
  src1 -= r0*mulF_h;
  uintex_t rem = ((uintex_t)src1 << 64) | src0;
  rem -= (uintex_t)r0 * mulF_l;

  const uintex_t mulF = ((uintex_t)mulF_h << 64) | mulF_l;
  if (rem >= mulF) {
    rem -= mulF;
    rx += 1;
  }
  int steaky = rem != 0;

  r1 = rx >> 64;
  r0 = rx;
#else
  uint64_t r1;
  uint64_t r0 = _umul128(srcH, invF_h, &r1);
  uint8_t carry;
  carry = _addcarry_u64(0,     r0, __umulh(srcH, invF_l), &r0);
  carry = _addcarry_u64(carry, r1, 0, &r1);
  carry = _addcarry_u64(0,     r0, __umulh(srcL, invF_h), &r0);
  carry = _addcarry_u64(carry, r1, 0, &r1);
  r0 = (r0 >> 2) | (r1 << (64-2));
  r1 = (r1 >> 2);

  src1 -= r1*mulF_l;
  src1 -= r0*mulF_h;
  uint64_t mx_h;
  uint64_t mx_l = _umul128(r0, mulF_l, &mx_h);
  uint8_t borrow;
  borrow = _subborrow_u64(0,      src0, mx_l, &src0);
  borrow = _subborrow_u64(borrow, src1, mx_h, &src1);
  // remainder in src1:src0

  uint64_t rem_h, rem_l;
  borrow = _subborrow_u64(0,      src0, mulF_l, &rem_l);
  borrow = _subborrow_u64(borrow, src1, mulF_h, &rem_h);
  if (!borrow) {
    src0 = rem_l;
    src1 = rem_h;
    carry = _addcarry_u64(0,     r0, 1, &r0);
    carry = _addcarry_u64(carry, r1, 0, &r1);
  }

  int steaky = (src0|src1) != 0;
#endif

  result[0] = (r1 << 63) | (r0 >> 1);
  result[1] = r1 >> 1;
  return (r0*2 & 2) | steaky;
}
