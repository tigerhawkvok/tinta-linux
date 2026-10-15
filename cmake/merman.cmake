# merman (https://github.com/Latias94/merman), the headless Rust port of
# Mermaid, built as a static library through its C ABI (merman-ffi).
# docs/linux-port/PLAN.md §6. Defines the `tinta_merman` interface target.
#
# Needs rustup and merman's pinned toolchain (rust-toolchain.toml), e.g.
# `rustup toolchain install 1.95.0`.

include(FetchContent)

# merman has no CMakeLists.txt: populate only
# The commit's source tarball, checked by hash: a shallow git clone only
# reaches a pinned commit while it is a branch tip. (Should GitHub ever
# re-encode archives again, the hash check fails loudly; refresh it.)
FetchContent_Declare(
    merman
    URL https://github.com/Latias94/merman/archive/3a4b8c393939b69372a8f05a2b971f7cdf6ae529.tar.gz  # 0.9.0
    URL_HASH SHA256=6bcca122c432f86efa17fbe07d56aada0420d185f4d774b261c032f700b85c2c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(merman)

# Corrosion calls the chosen toolchain's cargo directly, which bypasses
# rust-toolchain.toml, so hand it merman's pinned channel
if(NOT Rust_TOOLCHAIN)
    file(STRINGS ${merman_SOURCE_DIR}/rust-toolchain.toml _merman_channel
         REGEX "^channel *= *\"[^\"]+\"")
    string(REGEX REPLACE "^channel *= *\"([^\"]+)\".*" "\\1" _merman_channel "${_merman_channel}")
    set(Rust_TOOLCHAIN ${_merman_channel} CACHE STRING "rustup toolchain for merman")
endif()

FetchContent_Declare(
    corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG 1499b14e4906a2890f5cee1547c8848db261753d  # v0.6.1, pinned by commit
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(corrosion)

# Every diagram family; SVG out; Cytoscape layouts (mindmap, architecture);
# math labels through RaTeX (MIT code, OFL-1.1 KaTeX fonts: ship the notice).
# No ELK (EPL-2.0).
set(TINTA_MERMAN_FEATURES all-diagrams svg layout-cytoscape math CACHE STRING
    "merman-ffi Cargo features")

corrosion_import_crate(
    MANIFEST_PATH ${merman_SOURCE_DIR}/Cargo.toml
    CRATES merman-ffi
    CRATE_TYPES staticlib
    NO_DEFAULT_FEATURES
    FEATURES ${TINTA_MERMAN_FEATURES}
    PROFILE release
    LOCKED
)

# Speed over size (PLAN.md §6.1 has the trade-off). panic stays unwind:
# merman turns panics into error results at the FFI boundary.
set(TINTA_MERMAN_OPT_LEVEL 3 CACHE STRING "merman-ffi Cargo opt-level")
corrosion_set_env_vars(merman_ffi
    CARGO_PROFILE_RELEASE_OPT_LEVEL=${TINTA_MERMAN_OPT_LEVEL}
    CARGO_PROFILE_RELEASE_LTO=fat
    CARGO_PROFILE_RELEASE_CODEGEN_UNITS=1
)

add_library(tinta_merman INTERFACE)
target_include_directories(tinta_merman INTERFACE ${merman_SOURCE_DIR}/crates/merman-ffi/include)
target_link_libraries(tinta_merman INTERFACE merman_ffi)
target_compile_definitions(tinta_merman INTERFACE TINTA_MERMAN=1)
if("math" IN_LIST TINTA_MERMAN_FEATURES)
    target_compile_definitions(tinta_merman INTERFACE TINTA_MERMAN_MATH=1)
endif()
