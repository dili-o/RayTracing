cmake_minimum_required (VERSION 3.13)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(RayTracer)
set(CMAKE_CXX_STANDARD 17)

# Ensure Vulkan SDK environment variable is correctly normalized
file(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" VULKAN_SDK)

# Find and include SPIRV-Reflect headers
set(SPIRV_REFLECT_INCLUDE_DIR "${VULKAN_SDK}/Source/SPIRV-Reflect")

file(GLOB_RECURSE SOURCE_LIST "Src/*.h" "Src/*.hpp" "Src/*.cpp" 
    "${SPIRV_REFLECT_INCLUDE_DIR}/spirv_reflect.c" "Vendor/simdjson/simdjson.cpp"
)

add_executable (${PROJECT_NAME} ${SOURCE_LIST} "Src/Scene.cpp" "Src/BVHNode.cpp" "Src/TLAS.cpp")

file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders/Spirv")
target_compile_definitions(${PROJECT_NAME} PRIVATE HOME_PATH="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(${PROJECT_NAME} PRIVATE ASSETS_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets")
target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Assets/Shaders/")


find_package(Vulkan REQUIRED)

if(Vulkan_FOUND)
    # Include Vulkan headers
    target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS})

    # Link against Vulkan library using keyword signature
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
add_subdirectory(Vendor/volk)
target_link_libraries(${PROJECT_NAME} PRIVATE volk_headers)
# spdlog
add_subdirectory(Vendor/spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
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

target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:DEBUG>:_DEBUG>
    CONSOLE
    UNICODE
    _UNICODE
    VEXPORT
    _CRT_SECURE_NO_WARNINGS
)
