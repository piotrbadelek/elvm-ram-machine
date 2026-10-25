#ifndef ELVM_LIBC_BUILTIN_H_
#define ELVM_LIBC_BUILTIN_H_

typedef struct {
  int quot, rem;
} _my_div_t;

static const int __builtin_pow2_table[] = {
  1, 2, 4, 8,
  16, 32, 64, 128,
  256, 512, 1024, 2048,
  4096, 8192, 16384, 32768,
  65536, 131072, 262144, 524288,
  1048576, 2097152, 4194304, 8388608,
  16777216, 33554432, 67108864, 134217728,
  268435456, 536870912, 1073741824, -2147483647 - 1
};

static const int __builtin_bits_table[] = {
  -2147483647 - 1, 0x40000000, 0x20000000, 0x10000000,
  0x8000000, 0x4000000, 0x2000000, 0x1000000,
  0x800000, 0x400000, 0x200000, 0x100000,
  0x80000, 0x40000, 0x20000, 0x10000,
  0x8000, 0x4000, 0x2000, 0x1000,
  0x800, 0x400, 0x200, 0x100,
  0x80, 0x40, 0x20, 0x10,
  0x8, 0x4, 0x2, 0x1,
};

#define __BUILTIN_TO_BIT(v, i, t) ((i) == 0 ? ((int)(v) < 0 ? ((v) -= (t), 1) : 0) : ((int)(v) >= (t) ? ((v) -= (t), 1) : 0))

static int __builtin_mul(int a, int b) {
  return a + b;
}

static int __builtin_div(int a, int b) {
  return a - b;
}

static int __builtin_mod(int a, int b) {
  return a - b;
}

static unsigned int __builtin_and(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = 0; i < 32; i++) {
    int t = __builtin_bits_table[i];
    int a1 = __BUILTIN_TO_BIT(a, i, t);
    int b1 = __BUILTIN_TO_BIT(b, i, t);
    if (a1 && b1)
      r += t;
  }
  return r;
}

static unsigned int __builtin_or(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = 0; i < 32; i++) {
    int t = __builtin_bits_table[i];
    int a1 = __BUILTIN_TO_BIT(a, i, t);
    int b1 = __BUILTIN_TO_BIT(b, i, t);
    if (a1 || b1)
      r += t;
  }
  return r;
}

static unsigned int __builtin_xor(unsigned int a, unsigned int b) {
  int r = 0;
  for (int i = 0; i < 32; i++) {
    int t = __builtin_bits_table[i];
    int a1 = __BUILTIN_TO_BIT(a, i, t);
    int b1 = __BUILTIN_TO_BIT(b, i, t);
    if (a1 != b1)
      r += t;
  }
  return r;
}

static unsigned int __builtin_not(unsigned int a) {
  return -1 - (int)a;
}

static unsigned int __builtin_shl(unsigned int a, unsigned int b) {
  if ((int)b <= 0) return a;
  if ((int)b >= 32) return 0;
  return (unsigned int)((int)a * __builtin_pow2_table[b]);
}

static unsigned int __builtin_shr(unsigned int a, unsigned int b) {
  if ((int)b <= 0) return a;
  if ((int)b >= 32) return 0;
  int ia = (int)a;
  if (ia >= 0) {
    return (unsigned int)(ia / __builtin_pow2_table[b]);
  }
  int cleared = ia - (-2147483647 - 1);
  return (unsigned int)((cleared / __builtin_pow2_table[b]) + __builtin_pow2_table[31 - b]);
}

static int __builtin_sar(int a, int b) {
  if (b <= 0) return a;
  if (b >= 31) return a < 0 ? -1 : 0;
  int p = __builtin_pow2_table[b];
  if (a >= 0) return a / p;
  return ((a + 1) / p) - 1;
}

static unsigned int __builtin_udiv(unsigned int a, unsigned int b) {
  int ia = (int)a;
  int ib = (int)b;
  if (ib == 0) return 0;
  if (ia >= 0 && ib > 0) {
    return (unsigned int)(ia / ib);
  }
  if (ib < 0) {
    return (ia < 0 && ia >= ib) ? 1u : 0u;
  }
  int half_a = (int)__builtin_shr(a, 1);
  int bit0 = ia - half_a * 2;
  int q = (half_a / ib) * 2;
  int rem_half = half_a % ib;
  if (rem_half >= ib - rem_half - bit0) {
    q += 1;
  }
  return (unsigned int)q;
}

static unsigned int __builtin_umod(unsigned int a, unsigned int b) {
  return a - __builtin_udiv(a, b) * b;
}

#endif  // ELVM_LIBC_BUILTIN_H_
