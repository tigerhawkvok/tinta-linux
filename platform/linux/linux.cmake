# Linux build (docs/linux-port/PLAN.md). The top-level CMakeLists.txt includes
# this on non-Windows hosts once SOURCES and HEADERS are defined, then stops;
# the Windows build never reads it.
#
# Phase 0: the platform-free core and its tests. The app target arrives with
# the Cairo/Pango/GTK compat layer.

set(TINTA_LINUX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/platform/linux)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED IMPORTED_TARGET glib-2.0)

# Shared code relies on MSVC's transitive standard includes
add_compile_options(
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${TINTA_LINUX_DIR}/include/tinta_prelude.h>")

# Linux stand-ins for the Win32 headers shared code includes
add_library(tinta_compat STATIC
    ${TINTA_LINUX_DIR}/src/win32_base.cpp
    ${TINTA_LINUX_DIR}/src/win32_input.cpp
)
target_include_directories(tinta_compat PUBLIC ${TINTA_LINUX_DIR}/include)
target_compile_definitions(tinta_compat PUBLIC UNICODE _UNICODE NOMINMAX)
target_link_libraries(tinta_compat PUBLIC PkgConfig::GLIB)

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

# Mermaid coverage against external corpora: tests/mermaid_corpus/README.md
add_executable(mermaid_corpus_probe EXCLUDE_FROM_ALL
    tests/mermaid_corpus/mermaid_corpus_probe.cpp ${TINTA_MERMAID_SOURCES})
target_include_directories(mermaid_corpus_probe PRIVATE include)

if(BUILD_TESTING)
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
