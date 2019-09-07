#include <stdint.h>

// DivideDecimal68ByPowerOf10_ref - Divide unsigned integer number by power of ten
//
// Arguments:
// result - result of division, 2 64-bit words, range [0:10**34-1], Little Endian
// src    - source (dividend), 4 64-bit words, range [0:10**68-1], Little Endian
// n      - decimal exponent of the divisor, i.e. divisor=10**n, range 0 to 34
// Return value:  0 when remainder of division ==0
//                1 when remainder of division >0 and < divisor/2,
//                2 when remainder == divisor/2,
//                3 when remainder > divisor/2
int DivideDecimal68ByPowerOf10_ref(uint64_t result[2], const uint64_t src[4], unsigned n);
