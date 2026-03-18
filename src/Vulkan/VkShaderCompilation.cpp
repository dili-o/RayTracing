#include "VkShaderCompilation.h"
#include "Core/Assert.hpp"
#include "Core/Exceptions.hpp"
#include "Core/MurmurHash.h"
#include "PCH.h"
#include "Platform/FileIO.h"
// Externals
#include "slang/slang-com-ptr.h"
#include "slang/slang.h"
#include <filesystem>

using std::string;
using std::vector;

static const string base_cache_dir = "ShaderCache\\";

#define DEBUG_SYMBOLS
#if _DEBUG
static const string cache_sub_dir = "Debug\\";
#else
static const std::string cache_sub_dir = "Release\\";
#endif

static const string cache_dir = base_cache_dir + cache_sub_dir;

namespace hlx {

static string get_expanded_shader_code(std::string_view path,
                                       vector<string> &file_paths) {
  bool file_in_path = false;
  for (u64 i = 0; i < file_paths.size(); ++i) {
    if (file_paths.at(i) == path) {
      // throw Exception("File \"" + string(path) + " is recursively included");
      file_in_path = true;
      break;
    }
  }

  if (!file_in_path)
    file_paths.push_back(std::string(path));

  string fileContents = ReadFileAsString(path);
  string fileDirectory = GetDirectoryFromFilePath(path);

  // Look for includes
  size_t lineStart = 0;
  while (true) {
    size_t lineEnd = fileContents.find('\n', lineStart);
    size_t lineLength = 0;
    if (lineEnd == string::npos)
      lineLength = string::npos;
    else
      lineLength = lineEnd - lineStart;

    string line = fileContents.substr(lineStart, lineLength);
    if (line.find("#include") == 0) {
      const size_t startQuote = line.find('\"');
      const size_t endQuote = line.find('\"', startQuote + 1);
      string includePath =
          line.substr(startQuote + 1, endQuote - startQuote - 1);
      string fullIncludePath = fileDirectory + includePath.c_str();
      if (FileExists(fullIncludePath.c_str()) == false)
        throw Exception("Couldn't find #included file \"" + fullIncludePath +
                        "\"");

      string includeCode =
          get_expanded_shader_code(fullIncludePath.c_str(), file_paths);
      fileContents.insert(lineEnd + 1, includeCode);
      lineEnd += includeCode.length();
    }

    if (lineEnd == string::npos)
      break;

    lineStart = lineEnd + 1;
  }

  return fileContents;
}

namespace fs = std::filesystem;
using namespace slang;

static IGlobalSession *global_session = nullptr;

void SlangCompiler::init() {
  if (!global_session) {
    createGlobalSession(&global_session);
  }
}

void SlangCompiler::shutdown() noexcept {
  if (global_session) {
    global_session->release();
  }
}

#pragma region VkCompileOptions
VkCompileOptions::VkCompileOptions() { reset(); }

void VkCompileOptions::add(const std::string_view name, u32 value) {
  HASSERT(num_defines < MaxDefines);

  name_offsets.at(num_defines) = buffer_idx;
  for (u32 i = 0; i < name.length(); ++i)
    buffer[buffer_idx++] = name[i];
  ++buffer_idx;

  std::string string_val = std::to_string(value);
  define_offsets[num_defines] = buffer_idx;
  for (u32 i = 0; i < string_val.length(); ++i)
    buffer[buffer_idx++] = string_val[i];
  ++buffer_idx;

  ++num_defines;
}

void VkCompileOptions::reset() {
  num_defines = 0;
  buffer_idx = 0;

  for (u32 i = 0; i < MaxDefines; ++i) {
    name_offsets.at(i) = 0xFFFFFFFF;
    define_offsets.at(i) = 0xFFFFFFFF;
  }

  memset(buffer, 0, BufferSize);
}
#pragma endregion(VkCompileOptions)

static string VkMakeShaderCacheName(std::string_view shader_code,
                                    std::string_view function_name,
                                    const VkCompileOptions *defines) {
  string hash_string = string(shader_code);
  hash_string += "\n";
  hash_string += function_name;
  hash_string += "\n";

  for (u32 i = 0; i < defines->num_defines; ++i) {
    hash_string += defines->buffer + defines->name_offsets[i];
    hash_string += defines->buffer + defines->define_offsets[i];
  }

  Hash hash_code =
      GenerateHash(hash_string.data(), int(hash_string.length()), 0);

  return cache_dir + hash_code.ToString() + ".cache";
}

void SlangCompiler::compile_code(std::string_view entry_point_name,
                                 std::string_view module_name,
                                 std::string_view path, ShaderBlob &blob,
                                 const VkCompileOptions &compile_opts) {
  fs::path fs_path = path;
  const string extension = fs_path.extension().string();
  const string parent_dir = fs_path.parent_path().string();

  string shader_code;
  vector<string> file_paths;
  shader_code = get_expanded_shader_code(path, file_paths);
  string cache_name =
      VkMakeShaderCacheName(shader_code, entry_point_name, &compile_opts);
  // Check if cached spirv exists
  if (FileExists(cache_name.c_str())) {
    File cache_file(cache_name.c_str(), File::OpenRead);

    // Copy spirv into shader blob
    const u64 spirv_size = cache_file.Size();
    HASSERT(spirv_size % 4 == 0);
    blob._data.resize(spirv_size / 4);
    cache_file.Read(spirv_size, blob._data.data());
    return;
  }

  // Create a session with a target
  TargetDesc target_desc{};
  target_desc.format = SLANG_SPIRV;
  target_desc.profile =
      global_session->findProfile("spirv_1_6+SPV_GOOGLE_user_type");

  SessionDesc session_desc = {};
  session_desc.targets = &target_desc;
  session_desc.targetCount = 1;
  std::array<const char *, 1> search_paths = {parent_dir.c_str()};
  session_desc.searchPaths = search_paths.data();
  session_desc.searchPathCount = 1;
  // Add compiler options
  std::vector<PreprocessorMacroDesc> preprocess_macros;
  preprocess_macros.push_back({"VK", nullptr});
  for (u32 i = 0; i < compile_opts.num_defines; ++i) {
    cstring name = compile_opts.buffer + compile_opts.name_offsets[i];
    cstring definition = compile_opts.buffer + compile_opts.define_offsets[i];
    preprocess_macros.push_back({name, definition});
  }

  session_desc.preprocessorMacroCount = preprocess_macros.size();
  session_desc.preprocessorMacros = preprocess_macros.data();
  session_desc.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;
  CompilerOptionEntry entries[] = {
      // CompilerOptionEntry
      //{
      //	.name = CompilerOptionName::VulkanEmitReflection,
      //	.value =
      //	{
      //		.kind = CompilerOptionValueKind::Int,
      //		.intValue0 = 1
      //	}
      // },
      CompilerOptionEntry{
          .name = CompilerOptionName::DebugInformation,
          .value = {.kind = CompilerOptionValueKind::Int,
#ifdef DEBUG_SYMBOLS
                    .intValue0 =
                        SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_MAXIMAL
#else
                    .intValue0 =
                        SlangDebugInfoLevel::SLANG_DEBUG_INFO_LEVEL_NONE
#endif // DEBUG_SYMBOLS
          }},
      CompilerOptionEntry{
          .name = CompilerOptionName::Optimization,
          .value = {.kind = CompilerOptionValueKind::Int,
#ifdef DEBUG_SYMBOLS
                    .intValue0 =
                        SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_DEFAULT
#else
                    .intValue0 =
                        SlangOptimizationLevel::SLANG_OPTIMIZATION_LEVEL_HIGH
#endif // DEBUG_SYMBOLS
          }},
      CompilerOptionEntry{.name = CompilerOptionName::DisableWarnings,
                          .value = {.kind = CompilerOptionValueKind::String,
                                    .stringValue0 = "15205"}}};
  session_desc.compilerOptionEntries = entries;
  session_desc.compilerOptionEntryCount = ArraySize(entries);

  Slang::ComPtr<ISession> session;
  global_session->createSession(session_desc, session.writeRef());

  // Load the shader module
  fs::path old_dir = fs::current_path();
  Slang::ComPtr<IBlob> diagnostics;
  SlangResult slang_res;
  Slang::ComPtr<IModule> module;

  module = session->loadModule(module_name.data(), diagnostics.writeRef());

  if (diagnostics) {
    string error_msg =
        static_cast<const char *>(diagnostics->getBufferPointer());
    throw Exception("SlangCompiler Error:\n" + error_msg);
  }

  // Find the entry point
  Slang::ComPtr<IEntryPoint> entry_point;
  module->findEntryPointByName(entry_point_name.data(), entry_point.writeRef());

  if (!entry_point) {
    string error_msg =
        std::format("SlangCompiler:\nEntry point name {} not found!",
                    entry_point_name.data());
    throw Exception(error_msg);
  }

  // Link the program
  std::array<IComponentType *, 2> components = {module, entry_point};
  Slang::ComPtr<IComponentType> program;
  slang_res = session->createCompositeComponentType(
      components.data(), components.size(), program.writeRef());
  if (SLANG_FAILED(slang_res)) {
    throw Exception(
        "SlangCompiler:\nFailed to create composite component type!");
  }

  Slang::ComPtr<IComponentType> linked_program;
  slang_res = program->link(linked_program.writeRef(), diagnostics.writeRef());

  if (diagnostics) {
    if (SLANG_FAILED(slang_res)) {
      string error_msg =
          static_cast<const char *>(diagnostics->getBufferPointer());
      throw Exception("SlangCompiler Error:\n" + error_msg);
    } else {
      std::printf("%s\n",
                  static_cast<const char *>(diagnostics->getBufferPointer()));
    }
  }

  // Get the compiled code
  Slang::ComPtr<IBlob> spirv_blob;
  slang_res = linked_program->getEntryPointCode(0, 0, spirv_blob.writeRef(),
                                                diagnostics.writeRef());

  if (diagnostics) {
    if (SLANG_FAILED(slang_res)) {
      string error_msg =
          static_cast<const char *>(diagnostics->getBufferPointer());
      throw Exception("SlangCompiler Error:\n" + error_msg);
    } else {
      std::printf("%s\n",
                  static_cast<const char *>(diagnostics->getBufferPointer()));
    }
  }

  const size_t size_in_bytes = spirv_blob->getBufferSize();
  // Parse spirv
  // SpvReflectShaderModule spirvModule = {};
  // SpvReflectResult spirvRes = spvReflectCreateShaderModule(
  //     size_in_bytes, spirv_blob->getBufferPointer(), &spirvModule);
  // if (spirvRes != SPV_REFLECT_RESULT_SUCCESS) {
  //   throw Exception(L"SpvReflect failed to parse spirv module");
  // }
  // u32 count = 0;
  // spirvRes = spvReflectEnumerateDescriptorSets(&spirvModule, &count, NULL);
  // if (spirvRes != SPV_REFLECT_RESULT_SUCCESS) {
  //   throw Exception(L"SpvReflect failed to parse spirv descriptor sets");
  // }
  //
  // vector<SpvReflectDescriptorSet *> sets(count);
  // spvReflectEnumerateDescriptorSets(&spirvModule, &count, sets.data());
  //
  // // Enumerate push constants
  // u32 pushCount = 0;
  // spvReflectEnumeratePushConstantBlocks(&spirvModule, &pushCount, nullptr);
  //
  // std::vector<SpvReflectBlockVariable *> pushConstants(pushCount);
  // spvReflectEnumeratePushConstantBlocks(&spirvModule, &pushCount,
  //                                       pushConstants.data());
  //
  // u32 inputCount = 0;
  // spvReflectEnumerateInputVariables(&spirvModule, &inputCount, nullptr);
  //
  // std::vector<SpvReflectInterfaceVariable *> inputVars(inputCount);
  // spvReflectEnumerateInputVariables(&spirvModule, &inputCount,
  //                                   inputVars.data());

  // Copy spirv into shader blob
  HASSERT(size_in_bytes % 4 == 0);
  blob._data.resize(size_in_bytes / 4);
  std::memcpy(blob._data.data(), spirv_blob->getBufferPointer(), size_in_bytes);
  // Create the cache file
  // Create the cache directory if it doesn't exist
  if (DirectoryExists(base_cache_dir.c_str()) == false)
    CreateNewDirectory(base_cache_dir.c_str());

  if (DirectoryExists(cache_dir.c_str()) == false)
    CreateNewDirectory(cache_dir.c_str());

  File cache_file(cache_name.c_str(), File::OpenWrite);
  cache_file.Write(size_in_bytes, spirv_blob->getBufferPointer());
}
} // namespace hlx
