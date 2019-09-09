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
// 1. It works only on Little Endian machines with sizeof(uint32_t)*2=sizeof(uint64_t)
// 2. When src >= 10**n * 2**112 the results are incorrect, but the call is still legal
//    in a sense that it causes no memory corruptions, traps or any other undefined actions
int DivideDecimal68ByPowerOf10(uint64_t result[2], const uint64_t src[4], unsigned n)
{
  enum { DIV1_NMAX = 4, NMAX = 34 };
  if (n <= DIV1_NMAX) {
    // 10**n < 2**64. Single-pass division
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
    if (n <= 2) {
      r1 = __umulh(d1, invF) >> rshift;
    } else {
      r1 = __umulh(src1, invF) >> (rshift-8);
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

  if (n > NMAX)
    return 0; // should not happen

  // DIV1_NMAX < n <= NMAX
  // 10**n > 2**128
  static const uint8_t nb_tab[NMAX-DIV1_NMAX] = { // maximal number of bits in src[] - 64*k, where k=2 or 3
    130 % 64, //  5 ( 2)
    133 % 64, //  6 ( 5)
    137 % 64, //  7 ( 9)
    140 % 64, //  8 (12)
    143 % 64, //  9 (15)
    147 % 64, // 10 (19)
    150 % 64, // 11 (22)
    153 % 64, // 12 (25)
    157 % 64, // 13 (29)
    160 % 64, // 14 (32)
    163 % 64, // 15 (35)
    167 % 64, // 16 (39)
    170 % 64, // 17 (42)
    173 % 64, // 18 (45)
    177 % 64, // 19 (49)
    180 % 64, // 20 (52)
    183 % 64, // 21 (55)
    187 % 64, // 22 (59)
    190 % 64, // 23 (62)
    193 % 64, // 24 ( 1)
    196 % 64, // 25 ( 4)
    200 % 64, // 26 ( 8)
    203 % 64, // 27 (11)
    206 % 64, // 28 (14)
    210 % 64, // 29 (18)
    213 % 64, // 30 (21)
    216 % 64, // 31 (24)
    220 % 64, // 32 (28)
    223 % 64, // 33 (31)
    226 % 64, // 34 (34)
  };
  // Fetch upper 128 bits of the src[]
  unsigned nb = nb_tab[n-DIV1_NMAX-1];
  unsigned wi0 = n < 24 ? 0 : 1;
  uint64_t srcH = (src[wi0+2] << (64-nb)) | (src[wi0+1] >> nb);
  uint64_t srcL = (src[wi0+1] << (64-nb)) | (src[wi0+0] >> nb);

  static const struct {
    uint64_t mulF_l;  // 10**n / 2
    uint64_t mulF_h;
    uint64_t invF_l;  // 2**(nb+64*3+13) / mulF
    uint64_t invF_h;
  } recip_tab2[NMAX-DIV1_NMAX] = {
   {0x000000000000C350, 0x0000000000000000, 0x0FCF80DC33721D53, 0xA7C5AC471B478423 }, //  5
   {0x000000000007A120, 0x0000000000000000, 0xA63F9A49C2C1B10F, 0x8637BD05AF6C69B5 }, //  6
   {0x00000000004C4B40, 0x0000000000000000, 0x3D32907604691B4C, 0xD6BF94D5E57A42BC }, //  7
   {0x0000000002FAF080, 0x0000000000000000, 0xFDC20D2B36BA7C3D, 0xABCC77118461CEFC }, //  8
   {0x000000001DCD6500, 0x0000000000000000, 0x31680A88F8953030, 0x89705F4136B4A597 }, //  9
   {0x000000012A05F200, 0x0000000000000000, 0xB573440E5A884D1B, 0xDBE6FECEBDEDD5BE }, // 10
   {0x0000000BA43B7400, 0x0000000000000000, 0xF78F69A51539D748, 0xAFEBFF0BCB24AAFE }, // 11
   {0x000000746A528800, 0x0000000000000000, 0xF93F87B7442E45D3, 0x8CBCCC096F5088CB }, // 12
   {0x0000048C27395000, 0x0000000000000000, 0x2865A5F206B06FB9, 0xE12E13424BB40E13 }, // 13
   {0x00002D79883D2000, 0x0000000000000000, 0x538484C19EF38C94, 0xB424DC35095CD80F }, // 14
   {0x0001C6BF52634000, 0x0000000000000000, 0x0F9D37014BF60A10, 0x901D7CF73AB0ACD9 }, // 15
   {0x0011C37937E08000, 0x0000000000000000, 0x4C2EBE687989A9B3, 0xE69594BEC44DE15B }, // 16
   {0x00B1A2BC2EC50000, 0x0000000000000000, 0x09BEFEB9FAD487C2, 0xB877AA3236A4B449 }, // 17
   {0x06F05B59D3B20000, 0x0000000000000000, 0x3AFF322E62439FCF, 0x9392EE8E921D5D07 }, // 18
   {0x4563918244F40000, 0x0000000000000000, 0x2B31E9E3D06C32E5, 0xEC1E4A7DB69561A5 }, // 19
   {0xB5E3AF16B1880000, 0x0000000000000002, 0x88F4BB1CA6BCF584, 0xBCE5086492111AEA }, // 20
   {0x1AE4D6E2EF500000, 0x000000000000001B, 0xD3F6FC16EBCA5E03, 0x971DA05074DA7BEE }, // 21
   {0x0CF064DD59200000, 0x000000000000010F, 0x5324C68B12DD6338, 0xF1C90080BAF72CB1 }, // 22
   {0x8163F0A57B400000, 0x0000000000000A96, 0x75B7053C0F178293, 0xC16D9A0095928A27 }, // 23
   {0x0DE76676D0800000, 0x00000000000069E1, 0xC4926A9672793542, 0x9ABE14CD44753B52 }, // 24
   {0x8B0A00A425000000, 0x00000000000422CA, 0x9D41EEDEC1FA9102, 0x7BCB43D769F762A8 }, // 25
   {0x6E64066972000000, 0x0000000000295BE9, 0x95364AFE032A819D, 0xC612062576589DDA }, // 26
   {0x4FE8401E74000000, 0x00000000019D971E, 0x775EA264CF55347D, 0x9E74D1B791E07E48 }, // 27
   {0x1F12813088000000, 0x000000001027E72F, 0xC5E54EB70C4429FE, 0x7EC3DAF941806506 }, // 28
   {0x36B90BE550000000, 0x00000000A18F07D7, 0x096EE45813A04330, 0xCAD2F7F5359A3B3E }, // 29
   {0x233A76F520000000, 0x000000064F964E68, 0xA1258379A94D028D, 0xA2425FF75E14FC31 }, // 30
   {0x6048A59340000000, 0x0000003F1BDF1011, 0x80EACF948770CED7, 0x81CEB32C4B43FCF4 }, // 31
   {0xC2D677C080000000, 0x0000027716B6A0AD, 0x67DE18EDA5814AF2, 0xCFB11EAD453994BA }, // 32
   {0x9C60AD8500000000, 0x000018A6E32246C9, 0xECB1AD8AEACDD58E, 0xA6274BBDD0FADD61 }, // 33
   {0x1BC6C73200000000, 0x0000F684DF56C3E0, 0xBD5AF13BEF0B113E, 0x84EC3C97DA624AB4 }, // 34
  };
  const uint64_t invF_h = recip_tab2[n-DIV1_NMAX-1].invF_h;
  const uint64_t invF_l = recip_tab2[n-DIV1_NMAX-1].invF_l;
  const uint64_t mulF_h = recip_tab2[n-DIV1_NMAX-1].mulF_h;
  const uint64_t mulF_l = recip_tab2[n-DIV1_NMAX-1].mulF_l;
  uint64_t src1 = src[1];
  uint64_t src0 = src[0];

#ifndef _MSC_VER
  typedef unsigned __int128 uintex_t;
  uintex_t rx = (uintex_t)srcH * invF_h;
  // rx += __umulh(srcH, invF_l);
  rx += ((uintex_t)srcH * invF_l) >> 64;
  rx += __umulh(srcL, invF_h);
  rx >>= 13;
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
  r0 = (r0 >> 13) | (r1 << (64-13));
  r1 = (r1 >> 13);

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
