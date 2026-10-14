# Linux build (docs/linux-port/PLAN.md). The top-level CMakeLists.txt includes
# this on non-Windows hosts once SOURCES and HEADERS are defined, then stops;
# the Windows build never reads it.
#
# Phase 0: the platform-free core and its tests. The app target arrives with
# the Cairo/Pango/GTK compat layer.

set(TINTA_LINUX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0 gio-2.0)
pkg_check_modules(PANGOCAIRO REQUIRED IMPORTED_TARGET pangocairo pangoft2)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo cairo-pdf cairo-png)
pkg_check_modules(PIXBUF REQUIRED IMPORTED_TARGET gdk-pixbuf-2.0)
pkg_check_modules(RSVG REQUIRED IMPORTED_TARGET librsvg-2.0)
pkg_check_modules(FONTCONFIG REQUIRED IMPORTED_TARGET fontconfig)
# Remote images (URLDownloadToCacheFileW); without it they show as links
pkg_check_modules(CURL IMPORTED_TARGET libcurl)

# Shared code relies on MSVC's transitive standard includes
add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${TINTA_LINUX_DIR}/include/tinta_prelude.h>")

# Linux stand-ins for the Win32 headers shared code includes
add_library(tinta_compat STATIC
    ${TINTA_LINUX_DIR}/src/win32_base.cpp
    ${TINTA_LINUX_DIR}/src/win32_input.cpp
    ${TINTA_LINUX_DIR}/src/dwrite_pango.cpp
    ${TINTA_LINUX_DIR}/src/d2d_cairo.cpp
    ${TINTA_LINUX_DIR}/src/wic_imaging.cpp
    ${TINTA_LINUX_DIR}/src/com_streams.cpp
    ${TINTA_LINUX_DIR}/src/win32_window.cpp
    ${TINTA_LINUX_DIR}/src/win32_system.cpp
)
target_include_directories(tinta_compat PUBLIC ${TINTA_LINUX_DIR}/include)
target_compile_definitions(tinta_compat PUBLIC UNICODE _UNICODE NOMINMAX)
target_link_libraries(tinta_compat PUBLIC PkgConfig::GLIB PkgConfig::PANGOCAIRO PkgConfig::CAIRO
    PkgConfig::PIXBUF PkgConfig::RSVG PkgConfig::FONTCONFIG)
# Bundled fonts (Selawik for Segoe UI): found beside an installed binary in
# share/tinta/fonts, or in the source tree when run from a build
target_compile_definitions(tinta_compat PRIVATE
    TINTA_SOURCE_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/ DESTINATION share/tinta/fonts)
if(CURL_FOUND)
    target_link_libraries(tinta_compat PUBLIC PkgConfig::CURL)
    target_compile_definitions(tinta_compat PRIVATE TINTA_HAVE_CURL)
endif()

set(TINTA_MERMAID_SOURCES
    src/mermaid.cpp
    src/mermaid_ext.cpp
    src/mermaid_seq.cpp
    src/mermaid_pie.cpp
    src/mermaid_uml.cpp
    src/mermaid_git.cpp
    src/mermaid_gantt.cpp
    src/mermaid_charts.cpp
    src/mermaid_narrative.cpp
    src/mermaid_grid.cpp
    src/mermaid_charts2.cpp
    src/mermaid_block.cpp
    src/mermaid_c4.cpp
)

# The app minus its Windows entry point, on the compat layer (phase 1)
set(TINTA_CORE_SOURCES ${SOURCES})
list(REMOVE_ITEM TINTA_CORE_SOURCES src/main_d2d.cpp)
add_library(tinta_core STATIC ${TINTA_CORE_SOURCES})
target_include_directories(tinta_core PUBLIC include src ${md4c_SOURCE_DIR}/src)
target_link_libraries(tinta_core PUBLIC md4c tinta_compat)
target_compile_definitions(tinta_core PUBLIC
    TINTA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    TINTA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    TINTA_VERSION_PATCH=${PROJECT_VERSION_PATCH})

# The app: Tinta's own WinMain behind a Linux main(). Headless until the
# GTK window layer (phase 2): --render-png, --printpages and --export*.
add_executable(tinta ${TINTA_LINUX_DIR}/src/main_linux.cpp src/main_d2d.cpp)
target_include_directories(tinta PRIVATE ${TINTA_LINUX_DIR}/src)
target_link_libraries(tinta PRIVATE tinta_core)

# Mermaid through merman instead of the native parsers (PLAN.md §6)
option(TINTA_MERMAN "Render Mermaid with merman (needs rustup)" ON)
if(TINTA_MERMAN)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/merman.cmake)
endif()

# Mermaid coverage against external corpora: tests/mermaid_corpus/README.md
add_executable(mermaid_corpus_probe EXCLUDE_FROM_ALL
    tests/mermaid_corpus/mermaid_corpus_probe.cpp ${TINTA_MERMAID_SOURCES})
target_include_directories(mermaid_corpus_probe PRIVATE include)
if(TINTA_MERMAN)
    add_executable(merman_corpus_probe EXCLUDE_FROM_ALL
        tests/mermaid_corpus/mermaid_corpus_probe.cpp src/merman_engine.cpp
        ${TINTA_MERMAID_SOURCES})
    target_include_directories(merman_corpus_probe PRIVATE include)
    target_link_libraries(merman_corpus_probe PRIVATE tinta_merman)

    add_executable(merman_svg EXCLUDE_FROM_ALL
        tests/mermaid_corpus/merman_svg.cpp src/merman_engine.cpp)
    target_include_directories(merman_svg PRIVATE include)
    target_link_libraries(merman_svg PRIVATE tinta_merman)
endif()

if(BUILD_TESTING)
    include(${TINTA_LINUX_DIR}/upstream_tests.cmake)

    # Behaviour of the compat layer itself
    add_executable(compat_dwrite_tests ${TINTA_LINUX_DIR}/tests/dwrite_tests.cpp)
    target_link_libraries(compat_dwrite_tests PRIVATE tinta_compat)
    add_test(NAME compat_dwrite COMMAND compat_dwrite_tests)

    add_executable(compat_d2d_wic_tests ${TINTA_LINUX_DIR}/tests/d2d_wic_tests.cpp)
    target_include_directories(compat_d2d_wic_tests PRIVATE ${TINTA_LINUX_DIR}/src)
    find_package(ZLIB REQUIRED)  # reads the compressed PDF the print path writes
    target_link_libraries(compat_d2d_wic_tests PRIVATE tinta_compat ZLIB::ZLIB)
    add_test(NAME compat_d2d_wic COMMAND compat_d2d_wic_tests)

    # The whole app headless: an absolute path argument opens the document
    # (it is not taken for a /switch) and the window paints and saves
    set(TINTA_SMOKE_HOME ${CMAKE_BINARY_DIR}/linux-render/.ctest-home)
    add_test(NAME linux_headless_render
        COMMAND tinta ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/markdown-regression-control.md
            --render-png=${CMAKE_BINARY_DIR}/linux-render/ctest-control.png --size=900x700 --settle-ms=100)
    set_tests_properties(linux_headless_render PROPERTIES
        ENVIRONMENT "XDG_CONFIG_HOME=${TINTA_SMOKE_HOME}/config;XDG_CACHE_HOME=${TINTA_SMOKE_HOME}/cache;XDG_DATA_HOME=${TINTA_SMOKE_HOME}/data"
        PASS_REGULAR_EXPRESSION "render-png: window \"[^\"]*markdown-regression-control\\.md"
        FAIL_REGULAR_EXPRESSION "no painted window|\\[timeout\\]")

    add_executable(keymap_tests tests/keymap_tests.cpp src/keymap.cpp)
    target_include_directories(keymap_tests PRIVATE include)
    target_link_libraries(keymap_tests PRIVATE tinta_compat)
    add_test(NAME keymap COMMAND keymap_tests)

    add_executable(math_parser_tests tests/math_parser_tests.cpp
        src/math_parser.cpp src/math_macros.cpp)
    target_include_directories(math_parser_tests PRIVATE include src)
    add_test(NAME math_parser COMMAND math_parser_tests)

    add_executable(mermaid_tests tests/mermaid_tests.cpp ${TINTA_MERMAID_SOURCES})
    target_include_directories(mermaid_tests PRIVATE include)
    add_test(NAME mermaid_parser COMMAND mermaid_tests)

    add_executable(mermaid_fence_tests
        tests/mermaid_fence_tests.cpp
        ${TINTA_MERMAID_SOURCES}
        src/document.cpp src/frontmatter.cpp
        src/markdown.cpp src/footnotes.cpp
    )
    target_include_directories(mermaid_fence_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(mermaid_fence_tests PRIVATE md4c tinta_compat)
    target_compile_definitions(mermaid_fence_tests PRIVATE
        TINTA_MERMAID_PREAMBLE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mermaid-preamble.md")
    add_test(NAME mermaid_fences COMMAND mermaid_fence_tests)

    if(TINTA_MERMAN)
        add_executable(merman_engine_tests
            tests/merman_engine_tests.cpp
            src/merman_engine.cpp
            src/document.cpp src/frontmatter.cpp
            src/markdown.cpp src/footnotes.cpp
        )
        target_include_directories(merman_engine_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
        target_link_libraries(merman_engine_tests PRIVATE md4c tinta_compat tinta_merman)
        target_compile_definitions(merman_engine_tests PRIVATE
            TINTA_MERMAN_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mermaid-merman.md"
            TINTA_MERMAID_PREAMBLE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mermaid-preamble.md")
        add_test(NAME merman_engine COMMAND merman_engine_tests)
    endif()

    add_executable(document_tests
        tests/document_tests.cpp
        src/document.cpp src/frontmatter.cpp
        src/markdown.cpp src/footnotes.cpp
    )
    target_include_directories(document_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(document_tests PRIVATE md4c tinta_compat)
    add_test(NAME document_types COMMAND document_tests)

    add_executable(wordcount_tests
        tests/wordcount_tests.cpp
        src/wordcount.cpp
        src/document.cpp src/frontmatter.cpp
        src/markdown.cpp src/footnotes.cpp
    )
    target_include_directories(wordcount_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(wordcount_tests PRIVATE md4c tinta_compat)
    target_compile_definitions(wordcount_tests PRIVATE
        TINTA_COUNT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/word-count-240.md")
    add_test(NAME word_count COMMAND wordcount_tests)
endif()
