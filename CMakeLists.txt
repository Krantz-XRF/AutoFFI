cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project("AutoFFI")

find_package(LLVM REQUIRED)
find_package(Clang REQUIRED)

add_subdirectory(lava)

add_executable(AutoFFI)
file(GLOB AUTOFFI_SOURCES "src/*")
target_sources(AutoFFI PRIVATE ${AUTOFFI_SOURCES})
target_include_directories(AutoFFI SYSTEM PRIVATE "${LLVM_DIR}/../../../include")
target_compile_options(AutoFFI PRIVATE "-imsvc" "${LLVM_DIR}/../../../include")
target_link_directories(AutoFFI PRIVATE "${LLVM_DIR}/../../../lib")
target_link_libraries(AutoFFI PRIVATE lava-assert clangTooling clangBasic clangASTMatchers)
