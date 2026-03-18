#pragma once

namespace hlx {

// Compiled SPIRV code
struct ShaderBlob {
  // Helper functions
  [[nodiscard]]
  size_t size() const noexcept {
    return _data.size() * sizeof(u32);
  }
  [[nodiscard]]
  const u32 *data() const noexcept {
    return _data.data();
  }
  [[nodiscard]]
  u32 *data() noexcept {
    return _data.data();
  }

  std::vector<u32> _data;
};

struct VkCompileOptions {
public:
  static constexpr u32 MaxDefines = 16;
  static constexpr u32 BufferSize = 1024;

  VkCompileOptions();

  void add(const std::string_view name, u32 value);
  void reset();

public:
  std::array<u32, MaxDefines> name_offsets;
  std::array<u32, MaxDefines> define_offsets;
  char buffer[BufferSize];
  u32 num_defines;
  u32 buffer_idx;
};

class SlangCompiler {
public:
  static void init();
  static void shutdown() noexcept;

  static void
  compile_code(std::string_view entry_point_name, std::string_view module_name,
               std::string_view path, ShaderBlob &blob,
               const VkCompileOptions &compile_opts = VkCompileOptions());
};
} // namespace hlx
