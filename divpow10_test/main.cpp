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

static bool result_test(const uint64_t* inpv, const unsigned* expv, int nInps);
static void time_test(const uint64_t* inpv, const unsigned* expv, int nInps, int nIter);
static int FindNormalizationFactor(const uint64_t src[4]);
static void InitPow10Table(void);

static mp_uint128_t pow10_tab[35];

int main(int argz, char**argv)
{
  if (argz < 2)
  {
    fprintf(stderr,
      "divpow10_test - test speed and correctness of DivideUint224ByPowerOf10() routine.\n"
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

  InitPow10Table();

  std::vector<uint64_t> inpv(nInps*4);
  std::vector<unsigned> expv(nInps);

  std::mt19937_64 rndGen;
  std::uniform_int_distribution<uint64_t> rndDistr(0, uint64_t(-1));
  auto rndFunc = std::bind ( rndDistr, std::ref(rndGen) );

  //
  // Test 1 - random distribution of input width and scaling factor
  //
  const double NBITS_FACTOR = 3.3219280948873623478703194294894;
  for (int i = 0; i < nInps; ++i) {
    uint64_t ee = rndFunc();
    unsigned e1 = ((uint64_t(uint32_t(ee))*34) >> 32) + 1;       // 1:34
    unsigned e2 = ((uint64_t(uint32_t(ee>>32))*34) >> 32) + 1;   // 1:34
    unsigned e = e1 + e2;
    unsigned nBits = static_cast<unsigned>(ceil(NBITS_FACTOR*e));
    if (nBits > 224) nBits = 224;
    uint64_t x[4] = {0};
    unsigned nw = (nBits+63)/64;
    for (unsigned k = 0; k < nw; ++k)
      x[k] = rndFunc();
    unsigned nmsb = (nBits - 1) % 64;
    x[nw-1] = (x[nw-1] | (uint64_t(1) << 63)) >> (63 - nmsb); // exactly nmsb bits

    unsigned es = e > 34 ? e - 34 : e1;
    uint64_t y_ref[2];
    for (;;) {
      DivideUint224ByPowerOf10_ref(y_ref, x, es);
      if (y_ref[1] < (uint64_t(1) << 48))
        break; // no more than 112 bits
      ++es;
    }

    // store inputs for the tests
    for (int k = 0; k < 4; ++k)
      inpv[i*4+k] = x[k];
    expv[i] = es;
  }

  if (!result_test(inpv.data(), expv.data(), nInps))
    return 1;
  time_test(inpv.data(), expv.data(), nInps, nIter);

  //
  // Test 2 - another random distribution of input width and scaling factor
  //
  for (int i = 0; i < nInps; ++i) {
    uint64_t wx = rndFunc();
    uint64_t w0 = uint32_t(wx);
    uint64_t w1 = wx >> 32;
    int nBits = ((w0*224)>>32)+1;
    uint64_t x[4] = {0};
    unsigned nw = (nBits+63)/64;
    for (unsigned k = 0; k < nw; ++k)
      x[k] = rndFunc();
    unsigned nmsb = (nBits - 1) % 64;
    x[nw-1] = (x[nw-1] | (uint64_t(1) << 63)) >> (63 - nmsb); // exactly nmsb bits

    int min_exp = FindNormalizationFactor(x);
    expv[i] = (((35-min_exp)*w1) >> 32) + min_exp;

    // store inputs for the tests
    for (int k = 0; k < 4; ++k)
      inpv[i*4+k] = x[k];
  }

  if (!result_test(inpv.data(), expv.data(), nInps))
    return 1;
  time_test(inpv.data(), expv.data(), nInps, nIter);

  //
  // Test 3 - Input width and scaling factor are close to maximum
  //
  for (int i = 0; i < nInps; ++i) {
    for (unsigned k = 0; k < 4; ++k)
      inpv[i*4+k] = rndFunc();
    inpv[i*4+3] &= 0xFFFFFFFF;
    expv[i] = FindNormalizationFactor(&inpv[i*4]);
  }

  if (!result_test(inpv.data(), expv.data(), nInps))
    return 1;
  time_test(inpv.data(), expv.data(), nInps, nIter);

  return 0;
}


static void time_test(const uint64_t* inpv, const unsigned* expv, int nInps, int nIter)
{
  std::vector<int64_t> tmVec(nIter);
  uint64_t dummy = 0;
  for (int it = 0; it < nIter; ++it) {
    std::chrono::steady_clock::time_point hres_t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < nInps; ++i) {
      uint64_t y[2];
      int r = DivideUint224ByPowerOf10(y, &inpv[i*4], expv[i]);
      dummy ^= y[0];
      dummy ^= y[1];
      dummy ^= r;
    }
    std::chrono::steady_clock::time_point hres_t1 = std::chrono::steady_clock::now();
    tmVec[it] = std::chrono::duration_cast<std::chrono::microseconds>(hres_t1 - hres_t0).count();
  }
  int64_t ssum = 0;
  for (int i = 0; i < nInps; ++i)
    ssum += expv[i];
  // for (int it = 0; it < nIter; ++it)
    // printf("%2d %lld\n", it, tmVec[it]);

  std::nth_element(tmVec.begin(), tmVec.begin()+(nIter/2), tmVec.end());
  int64_t tmMed = tmVec[nIter/2];

  printf("%.1f ns/call. %lld usec total. Average scale %.2f.\n", tmMed*1e3/nInps, tmMed, double(ssum)/nInps);

  if (dummy==42)
    printf("Blue moon\n");
}

static bool result_test(const uint64_t* inpv, const unsigned* expv, int nInps)
{
  for (int i = 0; i < nInps; ++i) {
    int r_ref[5] = {0,1,2,3};
    uint64_t y_ref[2];
    r_ref[4] = DivideUint224ByPowerOf10_ref(y_ref, &inpv[i*4], expv[i]);
    mp_uint256_t x[5];
    if (expv[i] > 0) {
      x[0] = mulx(pow10_tab[expv[i]], y_ref);
      x[2] = add(x[0], pow10_tab[expv[i]].half());
      x[1] = sub(x[2], 1);
      x[3] = sub(add(x[0], pow10_tab[expv[i]]), 1);
    }
    x[4] = mp_uint256_t(&inpv[i*4]);
    for (int k = expv[i] > 0 ? 0 : 4; k < 5; ++k) {
      uint64_t y_res[2];
      int r_res = DivideUint224ByPowerOf10(y_res, x[k].w, expv[i]);
      if (y_res[0] != y_ref[0] || y_res[1] != y_ref[1] || r_res != r_ref[k]) {
        fprintf(stderr,
          "%016llx:%016llx:%016llx:%016llx / 1E%u\n"
          "res: %016llx:%016llx,%d\n"
          "ref: %016llx:%016llx,%d\n"
          "Fail!\n"
          ,x[k].w[3],x[k].w[2],x[k].w[1],x[k].w[0], expv[i]
          ,y_res[1],y_res[0], r_res
          ,y_ref[1],y_ref[0], r_ref[k]
          );
        return false;
      }
    }
 }
  return true;
}

static int FindNormalizationFactorCore(uint32_t x[8], int wi_max, uint32_t val_max)
{
  int n = 0;
  while (x[wi_max] > val_max) {
    uint64_t rem = 0;
    for (int wi = wi_max; wi >= 0; --wi) {
      uint64_t d = (rem << 32) | x[wi];
      x[wi] = (uint32_t)(d / 10);
      rem = d % 10;
    }
    ++n;
  }
  return n;
}
static int FindNormalizationFactor(const uint64_t src[4])
{
  uint32_t x[8];
  memcpy(x, src, sizeof(x));
  int n = 0;
  n += FindNormalizationFactorCore(x, 6, 0);
  n += FindNormalizationFactorCore(x, 5, 0);
  n += FindNormalizationFactorCore(x, 4, 0);
  n += FindNormalizationFactorCore(x, 3, 0xFFFF);
  return n;
}

static void InitPow10Table(void)
{
  mp_uint128_t val(1);
  for (int i = 0; i < sizeof(pow10_tab)/sizeof(pow10_tab[0]); ++i) {
    pow10_tab[i] = val;
    val *= 10;
  }
}