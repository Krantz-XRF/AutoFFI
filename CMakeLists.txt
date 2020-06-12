cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project("auto-ffi")

include(cmake/embed_files.cmake)

# LLVM/Clang dependency
find_package(LLVM REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}")
find_package(Clang REQUIRED)
message(STATUS "Using ClangConfig.cmake in ${Clang_DIR}")

# Use spdlog for logging information
find_package(spdlog REQUIRED)

# Use this fmt library in place of C++20 std::format
find_package(fmt 6 REQUIRED)

# Use string template engine inja
find_package(inja CONFIG REQUIRED)

# Use Microsoft.GSL
set(GSL_CXX_STANDARD 17)
add_subdirectory(GSL)

add_executable(auto-ffi)
add_subdirectory(src)
target_compile_features(auto-ffi PRIVATE cxx_std_17)

target_include_directories(auto-ffi SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_directories(auto-ffi PRIVATE ${LLVM_LIBRARY_DIRS})
target_link_libraries(auto-ffi PRIVATE
  clangTooling clangBasic clangASTMatchers
  fmt::fmt Microsoft.GSL::GSL spdlog::spdlog pantor::inja)

# Fix the wierd bug for nlohmann/json and Clang on Windows
# Should be fixed upstream in nlohmann/json
if("x${CMAKE_CXX_COMPILER_ID}" STREQUAL "xClang"
   AND "x${CMAKE_CXX_SIMULATE_ID}" STREQUAL "xMSVC")
  message(STATUS "ClangCL detected, define macros and/or/not for nlohmann/json")
  target_compile_definitions(auto-ffi PRIVATE -Dand=&& -Dor=|| -Dnot=!)
  target_compile_options(auto-ffi PRIVATE /EHsc)
endif()
