#include <cmath>
#include "multiprec_ut.h"
#ifdef _MSC_VER
#include <intrin.h>
#endif

#ifndef _MSC_VER
static inline uint64_t _umul128(uint64_t a, uint64_t b, uint64_t* xh) {
  unsigned __int128 x = (unsigned __int128)a * b;
  *xh = (uint64_t)(x>>64);
  return (uint64_t)x;
}
#endif

mp_uint128_t double2uint128(double x)
{
  int e;
  double m = frexp(x, &e);
  int re = e > 60 ? 60 : e;
  uint64_t w64 = uint64_t(ldexp(m, re));
  int ls = e - re;
  if (ls <= 0)  return mp_uint128_t(w64);
  if (ls < 64)  return mp_uint128_t(w64 << ls, w64 >> (64-ls));
  if (ls < 128) return mp_uint128_t(0,         w64 << (ls-64));
  return mp_uint128_t();
}

mp_uint128_t& mp_uint128_t::add(const mp_uint128_t& a)
{
  w[0] += a.w[0];
  w[1] += a.w[1] + (w[0] < a.w[0]);
  return *this;
}

mp_uint128_t& mp_uint128_t::mul(uint64_t a)
{
  uint64_t x00h;
  w[0] = _umul128(w[0], a, &x00h);
  w[1] = w[1]*a + x00h;
  return *this;
}

mp_uint128_t& mp_uint128_t::mul(const mp_uint128_t& a)
{
  uint64_t x00H;
  w[0] = _umul128(w[0], a.w[0], &x00H);
  w[1] = w[1]*a.w[0] + w[0]*a.w[1] + x00H;
  return *this;
}

mp_uint128_t mp_uint128_t::half() const
{
  return mp_uint128_t((w[0] >> 1)|(w[1] << 63), w[1] >> 1);
}

mp_uint256_t mulx(const mp_uint128_t& a, const mp_uint128_t& b)
{
  uint64_t x00H, x10H;
  uint64_t x00L = _umul128(a.w[0], b.w[0], &x00H);
  uint64_t x10L = _umul128(a.w[1], b.w[0], &x10H);
  uint64_t w0  = x00L;
  uint64_t w1a = x10L + x00H;
  uint64_t w2a = x10H + (w1a < x00H);

  uint64_t x01H, x11H;
  uint64_t x01L = _umul128(a.w[0], b.w[1], &x01H);
  uint64_t x11L = _umul128(a.w[1], b.w[1], &x11H);
  uint64_t w1b = x01L;
  uint64_t w2b = x11L + x01H;
  uint64_t w3  = x11H + (w2b < x01H);

  uint64_t w1 = w1a + w1b;
  uint64_t w2 = w2a + (w1 < w1b);
  w3 += (w2 < w2a);
  w2 += w2b;
  w3 += (w2 < w2b);

  return mp_uint256_t(w0, w1, w2, w3);
}

mp_uint256_t add(const mp_uint256_t& a, const mp_uint256_t& b)
{
  uint64_t w0  = a.w[0] + b.w[0];
  uint64_t w1a = a.w[1] + (w0 < a.w[0]);
  uint64_t w1  = w1a  + b.w[1];
  uint64_t w2a = a.w[2] + ((w1a < a.w[1]) | (w1 < w1a));
  uint64_t w2  = w2a  + b.w[2];
  uint64_t w3  = a.w[3] + b.w[3] + ((w2a < a.w[2]) | (w2 < w2a));

  return mp_uint256_t(w0, w1, w2, w3);
}

mp_uint256_t sub(const mp_uint256_t& a, const mp_uint256_t& b)
{
  uint64_t w0  = a.w[0] - b.w[0]; uint64_t c0 = a.w[0] < b.w[0];
  uint64_t w1a = a.w[1] - b.w[1]; uint64_t c1 = a.w[1] < b.w[1];
  uint64_t w2a = a.w[2] - b.w[2]; uint64_t c2 = a.w[2] < b.w[2];
  uint64_t w3a = a.w[3] - b.w[3];
  uint64_t w1  = w1a - c0; c1 |= (w1a < c0);
  uint64_t w2  = w2a - c1; c2 |= (w2a < c1);
  uint64_t w3  = w3a - c2;

  return mp_uint256_t(w0, w1, w2, w3);
}

mp_uint128_t mulu(const mp_uint128_t& a, const mp_uint128_t& b)
{
  mp_uint256_t ab = mulx(a, b);
  return mp_uint128_t(&ab.w[2]);
}
