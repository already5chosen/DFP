#include <vector>
#include <chrono>
#include <random>
#include <functional>
#include <algorithm>
#include <cstdio>
#include <cstring>

extern "C" {
#include "divide_pow10_reference.h"
#include "divide_pow10.h"
};
#include "multiprec_ut.h"

struct div_rem_t {
  mp_uint128_t div;
  mp_uint128_t rem;
};

static bool result_test(const mp_uint256_t* inpv, const unsigned* expv, const div_rem_t* outv, int nInps);
static void time_test(const mp_uint256_t* inpv, const unsigned* expv, int nInps, int nIter);
static void InitPow10Table(void);

static mp_uint128_t pow10_tab[35];

int main(int argz, char**argv)
{
  if (argz < 2)
  {
    fprintf(stderr,
      "divpow10_test - test speed and correctness of DivideDecimal68ByPowerOf10() routine.\n"
      "Usage:\n"
      "divpow10_test nInps [nIter]\n"
      "where\n"
      " nInps - # elements in test vector\n"
      " nIter - number of iterations. Default=17\n"
      );
    return 1;
  }

  char* endp;
  int nInps = strtol(argv[1], &endp, 0);
  if (endp == argv[1]) {
    fprintf(stderr, "Bad argument nInps='%s'. Not a number.\n", argv[1]);
    return 1;
  }

  int nIter = 17;
  if (argz >= 3) {
    nIter = strtol(argv[2], &endp, 0);
    if (endp == argv[2]) {
      fprintf(stderr, "Bad argument nIter='%s'. Not a number.\n", argv[2]);
      return 1;
    }
  }

  if (nInps < 1 || nInps > 1e8) {
    fprintf(stderr, "Bad argument nInps='%s'. Please specify number in range [1:100000000].\n", argv[1]);
    return 1;
  }

  if (nIter < 3 || nIter > 1000) {
    fprintf(stderr, "Bad argument nIter='%s'. Please specify number in range [3:1000].\n", argv[1]);
    return 1;
  }
  nIter |= 1; // use odd number of iterations
  nInps = (nInps + 1) & -2; // use even number of inputs

  InitPow10Table();

  std::vector<mp_uint256_t> inpv(nInps);
  std::vector<div_rem_t>    outv(nInps);
  std::vector<unsigned>     expv(nInps);

  std::mt19937_64 rndGen;
  std::uniform_int_distribution<uint64_t> rndDistr(0, uint64_t(-1));
  auto rndFunc = std::bind ( rndDistr, std::ref(rndGen) );

  static const unsigned n_ranges[][2] = {
    { 0, 34},
    { 1,  4},
    { 1, 19},
    {20, 27},
    {28, 34},
  };
  for (unsigned ri = 0; ri < sizeof(n_ranges)/sizeof(n_ranges[0]); ++ri) {
    unsigned r0 = n_ranges[ri][0];
    unsigned rl = n_ranges[ri][1]-r0+1;
    const uint64_t MSK32 = uint64_t(-1) >> (64-32);
    for (int i = 0; i < nInps; ++i) {
      uint64_t rndw[5];
      for (int k = 0; k < 5; ++k)
        rndw[k] = rndFunc();
      unsigned n = (((rndw[0] & MSK32)*rl) >> 32) + r0;
      mp_uint128_t xx; // result of division
      if (i % 2 == 1) { // distribution with log factor (biased by 8) on range [0:2**112-9]
        // use 44 random bits to generate log-distributed interval
        int64_t ex0 = (rndw[1] >> 20)*109 >> 12; // ex in range [0:119*2**32-1]
        double d_xx0 = floor(exp2((ex0+0)*(1.0/(uint64_t(1) << 32)))*8.0-8.0); // range [0:2**112-9]
        double d_xx1 = floor(exp2((ex0+1)*(1.0/(uint64_t(1) << 32)))*8.0-8.0);
        xx = double2uint128(d_xx0);
        double dULP = d_xx1 - d_xx0;
        if (dULP > 0) // use 84 remaining random bits to chose random point on interval [d_xx0:d_xx1)
          xx += mulu(double2uint128(dULP), mp_uint128_t(rndw[1] << 44, rndw[2]));
      } else { // uniform distribution on range [0:10**34-1]
        xx = mulu(pow10_tab[34], mp_uint128_t(&rndw[1]));
      }
      mp_uint128_t rx = mulu(pow10_tab[n],  mp_uint128_t(&rndw[3])); // remainder of division

      // store inputs for the tests
      outv[i].div = xx;
      outv[i].rem = rx;
      expv[i] = n;
      inpv[i] = add(mulx(pow10_tab[n], outv[i].div), outv[i].rem);
    }

    #if REPORT_UNDERFLOWS
    if (ri==0) {
      unsigned uu_cnt[35][12] = {{0}};
      for (int i = 0; i < nInps; ++i) {
        unsigned n = expv[i];
        mp_uint128_t offs = uint64_t(0);
        if (n > 0) {
          const double LOG2_10  = 3.3219280948873623478703194294894;
          unsigned re = floor(LOG2_10*n-1); // floor(log2(10**n/2))
          // generate offset exponentially distributed on range [2..2**re-1]
          // use 57 random bits to generate log-distributed interval
          int64_t ex0 = (rndFunc() >> 7)*(re-1) >> 25; // ex in range [0:(re-1)*2**32-1]
          double d_xx0 = floor(exp2((ex0+0)*(1.0/(uint64_t(1) << 32)))*2.0); // range [2..2**re-1]
          double d_xx1 = floor(exp2((ex0+1)*(1.0/(uint64_t(1) << 32)))*2.0);
          offs = double2uint128(d_xx0);
          double dULP = d_xx1 - d_xx0;
          if (dULP > 0) // use 128 random bits to chose random point on interval [d_xx0:d_xx1)
            offs += mulu(double2uint128(dULP), mp_uint128_t(rndFunc(), rndFunc()));
        }
        mp_uint256_t src[11];
        src[0]  = mulx(pow10_tab[n], outv[i].div);
        src[5]  = add(src[0], pow10_tab[n].half());
        src[1]  = add(src[0], 1);
        src[2]  = add(src[0], offs);
        src[3]  = sub(src[5], offs);
        src[4]  = sub(src[5], 1);
        src[6]  = add(src[5], 1);
        src[7]  = add(src[5], offs);
        src[8]  = add(src[3], pow10_tab[n].half());
        src[9]  = add(src[4], pow10_tab[n].half());
        src[10] = inpv[i];
        for (int k = 0; k < 11; ++k) {
          uint64_t dummy[2];
          DivideDecimal68ByPowerOf10(dummy, src[k].w, n);
          uu_cnt[n][k+1] += gl_underflow;
        }
        uu_cnt[n][0] += 1;
      }
      for (int i = 0; i < 35; ++i) {
        printf("%2d", i);
        for (int k = 0; k < 12; ++k)
          printf(" %6u", uu_cnt[i][k]);
        printf("\n");
      }
    }
    #endif

    if (!result_test(inpv.data(), expv.data(), outv.data(), nInps))
      return 1;
    time_test(inpv.data(), expv.data(), nInps, nIter);
  }

  return 0;
}

volatile uint64_t vo_zero;
static void time_test(const mp_uint256_t* inpv, const unsigned* expv, int nInps, int nIter)
{
  std::vector<int64_t> tmVec(nIter);
  // Throughput test
  uint64_t dummy = 0;
  for (int it = 0; it < nIter; ++it) {
    std::chrono::steady_clock::time_point hres_t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < nInps; ++i) {
      uint64_t y[2];
      int r = DivideDecimal68ByPowerOf10(y, inpv[i].w, expv[i]);
      dummy ^= y[0];
      dummy ^= y[1];
      dummy ^= r;
    }
    std::chrono::steady_clock::time_point hres_t1 = std::chrono::steady_clock::now();
    tmVec[it] = std::chrono::duration_cast<std::chrono::microseconds>(hres_t1 - hres_t0).count();
  }
  std::nth_element(tmVec.begin(), tmVec.begin()+(nIter/2), tmVec.end());
  int64_t tmMed_t = tmVec[nIter/2];

  // Latency test
  for (int it = 0; it < nIter; ++it) {
    uint64_t zero = vo_zero;
    unsigned dummy_n = 0;
    std::chrono::steady_clock::time_point hres_t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < nInps; ++i) {
      uint64_t y[2];
      int r = DivideDecimal68ByPowerOf10(y, inpv[i].w, expv[i]+dummy_n);
      dummy ^= y[0];
      dummy ^= y[1];
      dummy ^= r;
      dummy_n = ((dummy & zero) != 0);
    }
    std::chrono::steady_clock::time_point hres_t1 = std::chrono::steady_clock::now();
    tmVec[it] = std::chrono::duration_cast<std::chrono::microseconds>(hres_t1 - hres_t0).count();
  }
  std::nth_element(tmVec.begin(), tmVec.begin()+(nIter/2), tmVec.end());
  int64_t tmMed_l = tmVec[nIter/2];

  int64_t ssum = 0;
  unsigned s_min = expv[0];
  unsigned s_max = expv[0];
  for (int i = 0; i < nInps; ++i) {
    unsigned s = expv[i];
    ssum += s;
    if (s_min > s) s_min = s;
    if (s_max < s) s_max = s;
  }

  printf("rThr= %5.2f ns/call. %8lld usec total. Lat= %5.2f ns/call. %8lld usec total. Scale= %2u to %2u, average %5.2f.\n"
    , tmMed_t*1e3/nInps
    , tmMed_t
    , tmMed_l*1e3/nInps
    , tmMed_l
    , s_min, s_max
    , double(ssum)/nInps
    );

  if (dummy==42)
    printf("Blue moon\n");
}

static int calc_ret(const mp_uint128_t& rem, const mp_uint128_t& divisor)
{
  if ((rem.w[0] | rem.w[1])==0) return 0;
  mp_uint256_t half = divisor.half();
  if (rem.w[1] < half.w[1]) return 1;
  if (rem.w[1] > half.w[1]) return 3;
  if (rem.w[0] < half.w[0]) return 1;
  if (rem.w[0] > half.w[0]) return 3;
  return 2;
}

static bool result_test(const mp_uint256_t* inpv, const unsigned* expv, const div_rem_t* outv, int nInps)
{
  for (int i = 0; i < nInps; ++i) {
    int r_ref[9] = {0, 0,1,2,3, 0,1,2,3};
    r_ref[0] = calc_ret(outv[i].rem, pow10_tab[expv[i]]);
    mp_uint256_t x[9];
    mp_uint128_t y[9];
    for (int k = 0; k < 5; ++k)
      y[k] = outv[i].div;
    for (int k = 5; k < 9; ++k)
      y[k] = mp_uint128_t(0, outv[i].div.w[1]);
    x[0] = mp_uint256_t(inpv[i]);
    int nk = expv[i] > 0 ? 9 : 1;
    if (nk > 1) {
      x[1] = mulx(pow10_tab[expv[i]], outv[i].div);
      x[3] = add(x[1], pow10_tab[expv[i]].half());
      x[2] = sub(x[3], 1);
      x[4] = sub(add(x[1], pow10_tab[expv[i]]), 1);

      x[5] = mulx(pow10_tab[expv[i]], y[5]);
      x[6] = add(x[5], 1);
      x[7] = add(x[5], pow10_tab[expv[i]].half());
      x[8] = add(x[7], 1);
    }
    for (int k = 0; k < nk; ++k) {
      uint64_t y_res[2];
      int r_res = DivideDecimal68ByPowerOf10(y_res, x[k].w, expv[i]);
      if (y_res[0] != y[k].w[0] || y_res[1] != y[k].w[1] || r_res != r_ref[k]) {
        fprintf(stderr,
          "%016llx:%016llx:%016llx:%016llx / 1E%u\n"
          "res: %016llx:%016llx,%d\n"
          "ref: %016llx:%016llx,%d\n"
          "Fail!\n"
          ,x[k].w[3],x[k].w[2],x[k].w[1],x[k].w[0], expv[i]
          ,y_res[1],y_res[0], r_res
          ,y[k].w[1], y[k].w[0], r_ref[k]
          );
        return false;
      }
    }
  }
  return true;
}

static void InitPow10Table(void)
{
  mp_uint128_t val(1);
  for (unsigned i = 0; i < sizeof(pow10_tab)/sizeof(pow10_tab[0]); ++i) {
    pow10_tab[i] = val;
    val *= 10;
  }
}