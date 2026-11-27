# Cross-compiles the Windows build on Linux with clang-cl and lld-link
# against the MSVC CRT and Windows SDK, as `xwin splat` lays them out
# (https://github.com/Jake-Shadle/xwin; downloading them means accepting
# Microsoft's license). Same headers, ABI and static CRT as an MSVC build.
# platform/windows/build-windows.sh sets this up; by hand:
#
#   cmake -S . -B build-win -G Ninja \
#       -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang-cl-xwin.cmake \
#       -DTINTA_LLVM_BIN=<dir with clang-cl, lld-link, llvm-lib, llvm-rc> \
#       -DTINTA_XWIN_SDK=<xwin splat output>
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

# try_compile projects re-read this file without the cache
foreach(var TINTA_LLVM_BIN TINTA_XWIN_SDK)
    if(NOT ${var} AND DEFINED ENV{${var}})
        set(${var} $ENV{${var}})
    endif()
    if(NOT ${var})
        message(FATAL_ERROR "Set ${var} (cmake/toolchains/clang-cl-xwin.cmake)")
    endif()
    set(ENV{${var}} ${${var}})
endforeach()

set(CMAKE_C_COMPILER ${TINTA_LLVM_BIN}/clang-cl)
set(CMAKE_CXX_COMPILER ${TINTA_LLVM_BIN}/clang-cl)
set(CMAKE_LINKER ${TINTA_LLVM_BIN}/lld-link)
set(CMAKE_AR ${TINTA_LLVM_BIN}/llvm-lib)
set(CMAKE_RC_COMPILER ${TINTA_LLVM_BIN}/llvm-rc)
set(CMAKE_C_COMPILER_TARGET x86_64-pc-windows-msvc)
set(CMAKE_CXX_COMPILER_TARGET x86_64-pc-windows-msvc)

set(_sdk ${TINTA_XWIN_SDK})
set(_includes "/imsvc${_sdk}/crt/include /imsvc${_sdk}/sdk/include/ucrt /imsvc${_sdk}/sdk/include/um \
/imsvc${_sdk}/sdk/include/shared /imsvc${_sdk}/sdk/include/winrt")
set(CMAKE_C_FLAGS_INIT "${_includes}")
set(CMAKE_CXX_FLAGS_INIT "${_includes}")
set(CMAKE_RC_FLAGS_INIT "/I${_sdk}/sdk/include/um /I${_sdk}/sdk/include/shared /I${_sdk}/crt/include")
set(_libpaths "/libpath:${_sdk}/crt/lib/x86_64 /libpath:${_sdk}/sdk/lib/um/x86_64 /libpath:${_sdk}/sdk/lib/ucrt/x86_64")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_libpaths}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_libpaths}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_libpaths}")

set(CMAKE_FIND_ROOT_PATH ${_sdk})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# xwin fetches release libraries only (no msvcrtd.lib)
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)

# merman (TINTA_MERMAN) builds for the same target through Corrosion
set(Rust_CARGO_TARGET x86_64-pc-windows-msvc)
