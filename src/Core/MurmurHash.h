//=================================================================================================
//
//  MJP's DX11 Sample Framework
//  http://mynameismjp.wordpress.com/
//
//  All code licensed under the MIT license
//
//=================================================================================================

#pragma once

#include "PCH.h"

namespace hlx {

struct Hash {
  u64 A;
  u64 B;

  Hash() : A(0), B(0) {}
  Hash(u64 a, u64 b) : A(a), B(b) {}

  std::string ToString() const;

  bool operator==(const Hash &other) { return A == other.A && B == other.B; }
};

Hash GenerateHash(const void *key, int len, u32 seed = 0);

} // namespace hlx
