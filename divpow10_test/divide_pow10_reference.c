#include "divide_pow10_reference.h"
#include <string.h>

// DivideUint224ByPowerOf10_ref - Divide unsigned integer number by power of ten
//
// Arguments:
// result - result of division, 2 64-bit words, up to 112 significant bits, Little Endian
// src    - source (dividend), 4 64-bit words, up to 224 significant bits, Little Endian
// n      - decimal exponent of the divisor, i.e. divisor=10**n, range 0 to 34
// Return value: -1 when reminder of division is < divisor/2,
//                0 when reminder = divisor/2,
//                1 when reminder > divisor/2,
//
// Comments: It works only on Little Endian machine with sizeof(uint32_t)*2=sizeof(uint64_t)
int DivideUint224ByPowerOf10_ref(uint64_t result[2], const uint64_t src[4], unsigned n)
{
  uint64_t steaky = 0;
  uint64_t rem    = 0;
  uint32_t x[8];
  memcpy(x, src, sizeof(x));
  while (n > 0) {
    steaky |= rem;
    rem = 0;
    for (int wi = 6; wi >= 0; --wi) {
      uint64_t d = (rem << 32) | x[wi];
      x[wi] = (uint32_t)(d / 10);
      rem = d % 10;
    }
    --n;
  }
  memcpy(result, x, sizeof(uint64_t)*2);
  return rem < 5 ? -1 : (rem > 5) || (steaky != 0);
}