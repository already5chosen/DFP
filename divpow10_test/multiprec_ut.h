#pragma once
#include <stdint.h>

struct mp_uint128_t {
  mp_uint128_t() { w[0]=w[1]=0; }
  mp_uint128_t(uint64_t a) { w[0]=a; w[1]=0; }
  mp_uint128_t(uint64_t w0, uint64_t w1) { w[0]=w0; w[1]=w1; }
  mp_uint128_t(const uint64_t a[2]) { w[0]=a[0]; w[1]=a[1]; }
  mp_uint128_t& operator+=(const mp_uint128_t& a) { return add(a); }
  mp_uint128_t& operator-=(const mp_uint128_t& a) { return sub(a); }
  mp_uint128_t& operator*=(const mp_uint128_t& a) { return mul(a); }
  mp_uint128_t& operator*=(uint64_t a)            { return mul(a); }
  mp_uint128_t& operator+=(uint64_t a)            { return add(a); }
  mp_uint128_t& operator-=(uint64_t a)            { return sub(a); }

  mp_uint128_t& add(const mp_uint128_t& a);
  mp_uint128_t& sub(const mp_uint128_t& a);
  mp_uint128_t& mul(uint64_t a);
  mp_uint128_t& mul(const mp_uint128_t& a);
  mp_uint128_t half() const;

  uint64_t w[2];
};

mp_uint128_t double2uint128(double a);

static inline int cmp(const mp_uint128_t& a, const mp_uint128_t& b) {
  if (a.w[1] > b.w[1]) return 1;
  if (a.w[1] < b.w[1]) return -1;
  if (a.w[0] > b.w[0]) return 1;
  if (a.w[0] < b.w[0]) return -1;
  return 0;
}

struct mp_uint256_t {
  mp_uint256_t() { w[0]=w[1]=w[2]=w[3]=0; }
  mp_uint256_t(uint64_t a) { w[0]=a; w[1]=w[2]=w[3]=0; }
  mp_uint256_t(uint64_t w0,uint64_t w1,uint64_t w2,uint64_t w3) { w[0]=w0; w[1]=w1; w[2]= w2; w[3]=w3; }
  mp_uint256_t(const mp_uint128_t& a) { w[0]=a.w[0]; w[1]=a.w[1]; w[2]=w[3]=0; }
  mp_uint256_t(const uint64_t a[3])   { w[0]=a[0]; w[1]=a[1]; w[2]=a[2]; w[3]=a[3]; }

  uint64_t w[4];
};

mp_uint256_t mulx(const mp_uint128_t& a, const mp_uint128_t& b);
mp_uint256_t add(const mp_uint256_t& a, const mp_uint256_t& b);
mp_uint256_t sub(const mp_uint256_t& a, const mp_uint256_t& b);
mp_uint128_t mulu(const mp_uint128_t& a, const mp_uint128_t& b);
