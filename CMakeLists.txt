cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project("AutoFFI")

find_package(LLVM REQUIRED)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in ${LLVM_DIR}")
find_package(Clang REQUIRED)
message(STATUS "Using ClangConfig.cmake in ${Clang_DIR}")
# Use this fmt library in place of C++20 std::format
find_package(fmt 6 REQUIRED)

add_executable(AutoFFI)
add_subdirectory(src)

target_include_directories(AutoFFI SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_link_directories(AutoFFI PRIVATE ${LLVM_LIBRARY_DIRS})
target_link_libraries(AutoFFI PRIVATE clangTooling clangBasic clangASTMatchers fmt::fmt)
