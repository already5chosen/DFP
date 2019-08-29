#include <stdint.h>

// DivideUint224ByPowerOf10 - Divide unsigned integer number by power of ten
//
// Arguments:
// result - result of division, 2 64-bit words, up to 112 significant bits, Little Endian
// src    - source (dividend), 4 64-bit words, up to 224 significant bits, Little Endian
// n      - decimal exponent of the divisor, i.e. divisor=10**n, range 0 to 34
// Return value: -1 when reminder of division is < divisor/2,
//                0 when reminder = divisor/2,
//                1 when reminder > divisor/2,
//
// Comments:
// 1. It works only on Little Endian machine with sizeof(uint32_t)*2=sizeof(uint64_t)
// 2. When src >= 10**n * 2**112 the results are incorrect, but the call is still legal
//    in a sense that it causes no memory corruptions, traps or any other undefined actions
int DivideUint224ByPowerOf10(uint64_t result[2], const uint64_t src[4], unsigned n);
