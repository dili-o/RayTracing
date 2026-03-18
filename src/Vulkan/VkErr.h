#pragma once

#include "Core/Assert.hpp"
#include <Vendor/volk/volk.h>

cstring VkResultString(VkResult res) noexcept;

#define VK_CHECK(call)                                                         \
  {                                                                            \
    const VkResult result_ = call;                                             \
    HASSERT_MSGS(result_ == VK_SUCCESS, "Error code: {}",                      \
                 VkResultString(result_));                                     \
  }
