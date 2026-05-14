//=================================================================================================
//
//  MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#include "MurmurHash.h"
#include "PCH.h"
#include <bit>
#include <cstdint>

namespace hlx {

std::string Hash::ToString() const {
  return std::to_string(A) + "_" + std::to_string(B);
}

#define BIG_CONSTANT(x) (x)

//-----------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

HLX_FINLINE u32 getblock(const u32 *p, int i) { return p[i]; }

HLX_FINLINE u64 getblock(const u64 *p, int i) { return p[i]; }

//-----------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

HLX_FINLINE u32 fmix(u32 h) {
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

//----------

HLX_FINLINE u64 fmix(u64 k) {
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xff51afd7ed558ccd);
  k ^= k >> 33;
  k *= BIG_CONSTANT(0xc4ceb9fe1a85ec53);
  k ^= k >> 33;

  return k;
}

//-----------------------------------------------------------------------------

Hash GenerateHash(const void *key, const int len, const u32 seed) {
  const uint8_t *data = (const uint8_t *)key;
  const int nblocks = len / 16;

  u64 h1 = seed;
  u64 h2 = seed;

  const u64 c1 = BIG_CONSTANT(0x87c37b91114253d5);
  const u64 c2 = BIG_CONSTANT(0x4cf5ad432745937f);

  //----------
  // body

  const u64 *blocks = (const u64 *)(data);

  for (int i = 0; i < nblocks; i++) {
    u64 k1 = getblock(blocks, i * 2 + 0);
    u64 k2 = getblock(blocks, i * 2 + 1);

    k1 *= c1;
    k1 = std::rotl(k1, 31);
    k1 *= c2;
    h1 ^= k1;

    h1 = std::rotl(h1, 27);
    h1 += h2;
    h1 = h1 * 5 + 0x52dce729;

    k2 *= c2;
    k2 = std::rotl(k2, 33);
    k2 *= c1;
    h2 ^= k2;

    h2 = std::rotl(h2, 31);
    h2 += h1;
    h2 = h2 * 5 + 0x38495ab5;
  }

  //----------
  // tail

  const uint8_t *tail = (const uint8_t *)(data + nblocks * 16);

  u64 k1 = 0;
  u64 k2 = 0;

  switch (len & 15) {
  case 15:
    k2 ^= u64(tail[14]) << 48;
  case 14:
    k2 ^= u64(tail[13]) << 40;
  case 13:
    k2 ^= u64(tail[12]) << 32;
  case 12:
    k2 ^= u64(tail[11]) << 24;
  case 11:
    k2 ^= u64(tail[10]) << 16;
  case 10:
    k2 ^= u64(tail[9]) << 8;
  case 9:
    k2 ^= u64(tail[8]) << 0;
    k2 *= c2;
    k2 = std::rotl(k2, 33);
    k2 *= c1;
    h2 ^= k2;

  case 8:
    k1 ^= u64(tail[7]) << 56;
  case 7:
    k1 ^= u64(tail[6]) << 48;
  case 6:
    k1 ^= u64(tail[5]) << 40;
  case 5:
    k1 ^= u64(tail[4]) << 32;
  case 4:
    k1 ^= u64(tail[3]) << 24;
  case 3:
    k1 ^= u64(tail[2]) << 16;
  case 2:
    k1 ^= u64(tail[1]) << 8;
  case 1:
    k1 ^= u64(tail[0]) << 0;
    k1 *= c1;
    k1 = std::rotl(k1, 31);
    k1 *= c2;
    h1 ^= k1;
  };

  //----------
  // finalization

  h1 ^= len;
  h2 ^= len;

  h1 += h2;
  h2 += h1;

  h1 = fmix(h1);
  h2 = fmix(h2);

  h1 += h2;
  h2 += h1;

  return Hash(h1, h2);
}

} // namespace hlx
