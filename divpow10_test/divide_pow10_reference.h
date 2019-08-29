#include <stdint.h>

// DivideUint224ByPowerOf10_ref - Divide unsigned integer number by power of ten
//
// Arguments:
// result - result of division, 2 64-bit words, up to 112 significant bits, Little Endian
// src    - source (dividend), 4 64-bit words, up to 224 significant bits, Little Endian
// n      - decimal exponent of the divisor, i.e. divisor=10**n, range 0 to 34
// Return value: -1 when reminder of division is < divisor/2,
//                0 when reminder = divisor/2,
//                1 when reminder > divisor/2,
int DivideUint224ByPowerOf10_ref(uint64_t result[2], const uint64_t src[4], unsigned n);
