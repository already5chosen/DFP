import math
for n in range(1, 35):
  nbits = math.log2(10)*(n+34)
  nbytes = math.ceil(nbits/8)
  offs = 0 if (nbytes < 16) else (nbytes - 16)
  if n ==3 :
    offs = 1
  if offs==3 :
    offs = 4
  M = 2**(128+32+offs*8)
  d = 0 if offs<4 else min(n-1, (offs-4)*8)
  div2 = (10**34 * M*2 - 1) // (10**(n+34) - 2**d)
  div22 = div2 // 2**96
  rem22 = div2 - div22 * 2**96
  div21 = rem22 // 2**32
  div20 = rem22 - div21 * 2**32
  rem_offs = (n - 1) // 8
  p10 = 10**n // 2 // 256**rem_offs
  p101 = p10 // 2**64
  p100 = p10 - p101 * 2**64
  src_offsLL = 24 if offs==0 else (0 if offs < 4 else offs - 4)
  steaky_msk = 256**rem_offs - 1
  shift_LL = (4-offs)*8 if offs > 0 and offs < 4 else 0
  print(" {%2d, %2d, %d, %2d, 0x%08x, 0x%016x, 0x%016x, 0x%016x, 0x%08x }, // %2d" % (offs, src_offsLL, rem_offs, shift_LL, div20, div21, div22, p100, steaky_msk, n))

