#!/bin/sh
# Cross-build pul-run.exe (x86_64-pc-windows-msvc) with Tinta's toolchain:
# lld-link, clang-cl and the xwin CRT/SDK, static CRT as merman's build uses.
set -eu
X=${TINTA_CROSS:-$HOME/.cache/tinta-cross}
SDK=$X/msvc-sdk
export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER=$X/bin/lld-link
export CC_x86_64_pc_windows_msvc=$X/bin/clang-cl
export CXX_x86_64_pc_windows_msvc=$X/bin/clang-cl
export AR_x86_64_pc_windows_msvc=$X/bin/llvm-lib
export CFLAGS_x86_64_pc_windows_msvc="/imsvc $SDK/crt/include /imsvc $SDK/sdk/include/ucrt /imsvc $SDK/sdk/include/um /imsvc $SDK/sdk/include/shared"
export CXXFLAGS_x86_64_pc_windows_msvc="$CFLAGS_x86_64_pc_windows_msvc"
export CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS="-Ctarget-feature=+crt-static -Lnative=$SDK/crt/lib/x86_64 -Lnative=$SDK/sdk/lib/um/x86_64 -Lnative=$SDK/sdk/lib/ucrt/x86_64"
cd ${PUL_EVAL:-$HOME/.cache/tinta-pul-eval}/runner
cargo +1.95.0 build --release --bin pul-run --target x86_64-pc-windows-msvc --target-dir target-win 2>&1 | tail -25
ls -la target-win/x86_64-pc-windows-msvc/release/*.exe target-win/x86_64-pc-windows-msvc/release/*.dll 2>/dev/null || true
