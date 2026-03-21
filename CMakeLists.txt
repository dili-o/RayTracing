cmake_minimum_required (VERSION 3.13)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(RayTracer)
set(CMAKE_CXX_STANDARD 20)

# Ensure Vulkan SDK environment variable is correctly normalized
file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" VULKAN_SDK)

# Find and include SPIRV-Reflect headers
set(SPIRV_REFLECT_INCLUDE_DIR "${VULKAN_SDK}/Source/SPIRV-Reflect")

file(GLOB_RECURSE SOURCE_LIST "Src/*.h" "Src/*.hpp" "Src/*.cpp" 
  "${SPIRV_REFLECT_INCLUDE_DIR}/spirv_reflect.c" "Vendor/simdjson/simdjson.cpp"
)
# Imgui
file(GLOB IMGUI_SOURCE_LIST 
  "${CMAKE_CURRENT_SOURCE_DIR}/Vendor/imgui/*.cpp"
)

add_executable (${PROJECT_NAME} ${SOURCE_LIST} ${IMGUI_SOURCE_LIST} "Src/Scene.cpp" "Src/BVHNode.cpp" "Src/TLAS.cpp")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders/Spirv")
target_compile_definitions(${PROJECT_NAME} PRIVATE HOME_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets")
target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders/")


find_package(Vulkan REQUIRED)

if(Vulkan_FOUND)
  target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${Vulkan_LIBRARIES})
else()
  message(FATAL_ERROR "Vulkan SDK not found. Make sure it is installed and accessible.")
endif()

# ----------------- Vendors
# volk
if(WIN32)
  target_compile_definitions(${PROJECT_NAME} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
  set(VOLK_STATIC_DEFINES VK_USE_PLATFORM_WIN32_KHR)
endif()
# spdlog
add_subdirectory(Vendor/spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
# sdl
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
add_subdirectory(Vendor/sdl EXCLUDE_FROM_ALL)
target_link_libraries(${PROJECT_NAME} PUBLIC SDL3-shared)
# GLM
set(GLM_BUILD_TESTS OFF CACHE BOOL "Disable GLM tests" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Use static GLM library" FORCE)
add_subdirectory(Vendor/glm)
target_link_libraries(${PROJECT_NAME} PUBLIC glm::glm)
target_include_directories(${PROJECT_NAME} PUBLIC Vendor/glm)
# Slang
target_link_libraries(${PROJECT_NAME} PRIVATE "${VULKAN_SDK}/Lib/slang.lib")
# -------------------------
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/")
target_include_directories(${PROJECT_NAME} PRIVATE "Src/")
target_include_directories(${PROJECT_NAME} PRIVATE "${SPIRV_REFLECT_INCLUDE_DIR}/")
target_include_directories(${PROJECT_NAME} PRIVATE "Vendor/")
# Set output directory to Bin/Engine
set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bin/$<CONFIG>/${PROJECT_NAME}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bin/$<CONFIG>/${PROJECT_NAME}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Bin/$<CONFIG>/${PROJECT_NAME}"
)
# PCH
target_precompile_headers(${PROJECT_NAME} PRIVATE
  "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/Src/PCH.h>")

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:DEBUG>:_DEBUG>
    CONSOLE
    UNICODE
    _UNICODE
    VEXPORT
    _CRT_SECURE_NO_WARNINGS
    VK_NO_PROTOTYPES
    MAX_FRAMES_IN_FLIGHT=2
    VULKAN_DEBUG_NAMES
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${VULKAN_SDK}/Bin/slang.dll"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "$<TARGET_FILE:SDL3-shared>"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
  COMMENT "Copying DLLs to output directory"
)
