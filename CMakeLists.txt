cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project("AutoFFI")

include(FindLua)
find_package(lua REQUIRED)
find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)
# Use this fmt library in place of C++20 std::format
find_package(fmt REQUIRED)

add_subdirectory(lava)

add_executable(AutoFFI)
add_subdirectory(src)

target_include_directories(AutoFFI SYSTEM PRIVATE
  "${LLVM_DIR}/../../../include" ${LUA_INCLUDE_DIR})
target_link_directories(AutoFFI PRIVATE "${LLVM_DIR}/../../../lib")
target_link_libraries(AutoFFI PRIVATE lava-assert
  clangTooling clangBasic clangASTMatchers ${LUA_LIBRARIES} fmt::fmt)
