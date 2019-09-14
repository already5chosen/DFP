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
    int8_t   srcH_offs; // maximal number of bits in src[] rounded up to the whole octet - 128 / 8
    uint8_t  srcH_msk;
    uint8_t  rsrv[6];
    uint64_t mulF_l;  // 10**n / 2
    uint64_t mulF_h;
    uint64_t invF_l;  // 2**(srcH_offs*8+130) / mulF
    uint64_t invF_h;
  } recip_tab[NMAX] = {
   { -1, 255, {0}, 0x0000000000000005, 0x0000000000000000, 0xCCCCCCCCCCCCCCCC, 0x0CCCCCCCCCCCCCCC }, //  1
   { -1, 255, {0}, 0x0000000000000032, 0x0000000000000000, 0x147AE147AE147AE1, 0x0147AE147AE147AE }, //  2
   {  0,   0, {0}, 0x00000000000001F4, 0x0000000000000000, 0xD916872B020C49BA, 0x20C49BA5E353F7CE }, //  3
   {  0,   0, {0}, 0x0000000000001388, 0x0000000000000000, 0xAF4F0D844D013A92, 0x0346DC5D63886594 }, //  4
   {  1,   0, {0}, 0x000000000000C350, 0x0000000000000000, 0x87E7C06E19B90EA9, 0x53E2D6238DA3C211 }, //  5
   {  1,   0, {0}, 0x000000000007A120, 0x0000000000000000, 0x5A63F9A49C2C1B10, 0x08637BD05AF6C69B }, //  6
   {  2,   0, {0}, 0x00000000004C4B40, 0x0000000000000000, 0x3D32907604691B4C, 0xD6BF94D5E57A42BC }, //  7
   {  2,   0, {0}, 0x0000000002FAF080, 0x0000000000000000, 0x9FB841A566D74F87, 0x15798EE2308C39DF }, //  8
   {  2,   0, {0}, 0x000000001DCD6500, 0x0000000000000000, 0x5CC5A02A23E254C0, 0x0225C17D04DAD296 }, //  9
   {  3,   0, {0}, 0x000000012A05F200, 0x0000000000000000, 0xAD5CD10396A21346, 0x36F9BFB3AF7B756F }, // 10
   {  3,   0, {0}, 0x0000000BA43B7400, 0x0000000000000000, 0xF7BC7B4D28A9CEBA, 0x057F5FF85E592557 }, // 11
   {  4,   0, {0}, 0x000000746A528800, 0x0000000000000000, 0xF93F87B7442E45D3, 0x8CBCCC096F5088CB }, // 12
   {  4,   0, {0}, 0x0000048C27395000, 0x0000000000000000, 0x32865A5F206B06FB, 0x0E12E13424BB40E1 }, // 13
   {  4,   0, {0}, 0x00002D79883D2000, 0x0000000000000000, 0x1EA70909833DE719, 0x016849B86A12B9B0 }, // 14
   {  5,   0, {0}, 0x0001C6BF52634000, 0x0000000000000000, 0x43E74DC052FD8284, 0x24075F3DCEAC2B36 }, // 15
   {  5,   0, {0}, 0x0011C37937E08000, 0x0000000000000000, 0x6D30BAF9A1E626A6, 0x039A5652FB113785 }, // 16
   {  6,   0, {0}, 0x00B1A2BC2EC50000, 0x0000000000000000, 0x84DF7F5CFD6A43E1, 0x5C3BD5191B525A24 }, // 17
   {  6,   0, {0}, 0x06F05B59D3B20000, 0x0000000000000000, 0x73AFF322E62439FC, 0x09392EE8E921D5D0 }, // 18
   {  7,   0, {0}, 0x4563918244F40000, 0x0000000000000000, 0x2B31E9E3D06C32E5, 0xEC1E4A7DB69561A5 }, // 19
   {  7,   0, {0}, 0xB5E3AF16B1880000, 0x0000000000000002, 0x511E976394D79EB0, 0x179CA10C9242235D }, // 20
   {  7,   0, {0}, 0x1AE4D6E2EF500000, 0x000000000000001B, 0xBB4FDBF05BAF2978, 0x025C768141D369EF }, // 21
   {  8,   0, {0}, 0x0CF064DD59200000, 0x000000000000010F, 0x54C931A2C4B758CE, 0x3C7240202EBDCB2C }, // 22
   {  8,   0, {0}, 0x8163F0A57B400000, 0x0000000000000A96, 0x3BADB829E078BC14, 0x060B6CD004AC9451 }, // 23
   {  9,   0, {0}, 0x0DE76676D0800000, 0x00000000000069E1, 0xC4926A9672793542, 0x9ABE14CD44753B52 }, // 24
   {  9,   0, {0}, 0x8B0A00A425000000, 0x00000000000422CA, 0x13A83DDBD83F5220, 0x0F79687AED3EEC55 }, // 25
   {  9,   0, {0}, 0x6E64066972000000, 0x0000000000295BE9, 0xB52A6C95FC065503, 0x018C240C4AECB13B }, // 26
   { 10,   0, {0}, 0x4FE8401E74000000, 0x00000000019D971E, 0x1DD7A89933D54D1F, 0x279D346DE4781F92 }, // 27
   { 10,   0, {0}, 0x1F12813088000000, 0x000000001027E72F, 0x362F2A75B862214F, 0x03F61ED7CA0C0328 }, // 28
   { 11,   0, {0}, 0x36B90BE550000000, 0x00000000A18F07D7, 0x04B7722C09D02198, 0x65697BFA9ACD1D9F }, // 29
   { 11,   0, {0}, 0x233A76F520000000, 0x000000064F964E68, 0x1A1258379A94D028, 0x0A2425FF75E14FC3 }, // 30
   { 11,   0, {0}, 0x6048A59340000000, 0x0000003F1BDF1011, 0xE901D59F290EE19D, 0x01039D66589687F9 }, // 31
   { 12,   0, {0}, 0xC2D677C080000000, 0x0000027716B6A0AD, 0x4CFBC31DB4B0295E, 0x19F623D5A8A73297 }, // 32
   { 12,   0, {0}, 0x9C60AD8500000000, 0x000018A6E32246C9, 0x87B2C6B62BAB3756, 0x02989D2EF743EB75 }, // 33
   { 13,   0, {0}, 0x1BC6C73200000000, 0x0000F684DF56C3E0, 0x5EAD789DF785889F, 0x42761E4BED31255A }, // 34
  };

  // Fetch upper 128 bits of the src[]
  // Attention this code works only on byte-addressable Little Endian machines!
  const int srcH_offs = recip_tab[n-1].srcH_offs;
  uint64_t srcH, srcL;
  memcpy(&srcH, (char*)src + srcH_offs + 8, sizeof(srcH));
  memcpy(&srcL, (char*)src + srcH_offs + 1, sizeof(srcL));
  srcL = (srcL << 8) | (*(uint8_t*)((char*)src + (srcH_offs & 15)) | recip_tab[n-1].srcH_msk);

  const uint64_t invF_h = recip_tab[n-1].invF_h;
  const uint64_t invF_l = recip_tab[n-1].invF_l;
  const uint64_t mulF_h = recip_tab[n-1].mulF_h;
  const uint64_t mulF_l = recip_tab[n-1].mulF_l;
  uint64_t src1 = src[1];
  uint64_t src0 = src[0];

#ifndef _MSC_VER
  typedef unsigned __int128 uintex_t;
  uintex_t rx = (uintex_t)srcH * invF_h;
  rx += ((uintex_t)srcH * invF_l) >> 64;
  rx += ((uintex_t)srcL * invF_h) >> 64;
  rx >>= 6;
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
    #if REPORT_UNDERFLOWS
    gl_underflow = 1;
    #endif
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
  r0 = (r0 >> 6) | (r1 << (64-6));
  r1 = (r1 >> 6);

  src1 -= r1*mulF_l;
  src1 -= r0*mulF_h;
  uint64_t mx_h;
  uint64_t mx_l = _umul128(r0, mulF_l, &mx_h);
  uint8_t borrow;
  borrow = _subborrow_u64(0,      src0, mx_l, &src0);
  borrow = _subborrow_u64(borrow, src1, mx_h, &src1);
  // remainder in src1:src0
  int steaky = (src0|src1) != 0;

  uint64_t rem_h, rem_l;
  borrow = _subborrow_u64(0,      src0, mulF_l, &rem_l);
  borrow = _subborrow_u64(borrow, src1, mulF_h, &rem_h);
  if (!borrow) {
    steaky = (rem_l|rem_h) != 0;
    carry = _addcarry_u64(0,     r0, 1, &r0);
    carry = _addcarry_u64(carry, r1, 0, &r1);
  }
#endif

  result[0] = (r1 << 63) | (r0 >> 1);
  result[1] = r1 >> 1;
  return (r0*2 & 2) | steaky;
}
