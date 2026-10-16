cmake_minimum_required(VERSION 3.16)
project(HILBERT LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(HILBERT_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(HILBERT_BUILD_LIBFUZZER "Build the Clang libFuzzer kernel harness" OFF)

add_executable(hilbert HILBERT.cpp)
target_compile_options(hilbert PRIVATE -Wall -Wextra -Wpedantic)
if(HILBERT_SANITIZERS AND NOT MSVC)
  target_compile_options(hilbert PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(hilbert PRIVATE -fsanitize=address,undefined)
endif()

enable_testing()
add_test(NAME hilbert_regression COMMAND hilbert --test)

if(HILBERT_BUILD_LIBFUZZER)
  if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "HILBERT_BUILD_LIBFUZZER requires Clang")
  endif()
  add_executable(hilbert_fuzz_kernel fuzz_kernel.cpp)
  target_compile_options(hilbert_fuzz_kernel PRIVATE -Wall -Wextra -Wpedantic
    -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
  target_link_options(hilbert_fuzz_kernel PRIVATE -fsanitize=fuzzer,address,undefined)
endif()
