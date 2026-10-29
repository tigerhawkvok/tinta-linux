cmake_minimum_required(VERSION 3.15)
# The release version, compiled in as TINTA_VERSION_MAJOR/MINOR/PATCH.
# -DTINTA_VERSION=X.Y.Z overrides it for one configure
# (packaging/build-release.sh); the override is not kept, so the next
# plain configure of the tree comes back to this default.
set(TINTA_DEFAULT_VERSION 4.0.12)
if(DEFINED CACHE{TINTA_VERSION})
    set(TINTA_BUILD_VERSION "$CACHE{TINTA_VERSION}")
    unset(TINTA_VERSION CACHE)
else()
    set(TINTA_BUILD_VERSION ${TINTA_DEFAULT_VERSION})
endif()
if(NOT TINTA_BUILD_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "TINTA_VERSION must be three numbers, X.Y.Z, not \"${TINTA_BUILD_VERSION}\"")
endif()
project(tinta VERSION ${TINTA_BUILD_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

include(CTest)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
if(MSVC)
    add_compile_options(/W4)
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
    # Static CRT: the exe must run on clean Windows installs without the
    # VC++ redistributable (costs ~250 KB, keeps the single-file promise)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# MD4C markdown parser
include(FetchContent)
FetchContent_Declare(
    md4c
    GIT_REPOSITORY https://github.com/mity/md4c.git
    GIT_TAG 729e6b8b320caa96328968ab27d7db2235e4fb47  # release-0.5.2, pinned by commit
    GIT_SHALLOW TRUE
)
# md4c defaults to a shared library off Windows; Tinta links it in
if(NOT WIN32)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "md4c is linked statically" FORCE)
endif()
FetchContent_MakeAvailable(md4c)

# Source files
set(SOURCES
    src/main_d2d.cpp
    src/markdown.cpp src/footnotes.cpp
    src/themes.cpp
    src/settings.cpp
    src/keymap.cpp
    src/emacs_keys.cpp
    src/markdown_copy.cpp
    src/bundled_fonts.cpp
    src/config_dir.cpp
    src/signals.cpp
    src/tableedit.cpp
    src/table_format.cpp
    src/d2d_init.cpp
    src/syntax.cpp
    src/syntax_extended.cpp
    src/utils.cpp
    src/search.cpp
    src/search_input.cpp
    src/search_panel.cpp
    src/render.cpp
    src/image_loader.cpp
    src/file_utils.cpp
    src/overlays.cpp
    src/frontmatter_ui.cpp
    src/sidepanels.cpp
    src/context_menu.cpp
    src/input.cpp
    src/editor.cpp
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
    src/document.cpp src/frontmatter.cpp
    src/inline_style.cpp
    src/print.cpp
    src/i18n.cpp
    src/selection.cpp
    src/math_render.cpp
    src/math_parser.cpp
    src/math_macros.cpp
    src/tabs.cpp
    src/export_html.cpp
    src/export_docx.cpp
    src/annotations.cpp
    src/drafts.cpp
    src/startpage.cpp
    src/editrail.cpp
    src/pandoc.cpp
    src/plantuml.cpp
    src/plantuml_queue.cpp
    src/plantuml_app.cpp
    src/wordcount.cpp
)

# The third-party notices, built into the binary (Settings > Updates >
# Licenses). Regenerate the .md with packaging/notices/generate_notices.py.
set(TINTA_NOTICES_CPP ${CMAKE_CURRENT_BINARY_DIR}/generated/notices_data.cpp)
add_custom_command(OUTPUT ${TINTA_NOTICES_CPP}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/packaging/THIRD-PARTY-NOTICES.md
            -DOUTPUT=${TINTA_NOTICES_CPP} -DSYMBOL=kThirdPartyNotices
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/packaging/THIRD-PARTY-NOTICES.md
            ${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_file.cmake
    COMMENT "Embedding the third-party notices")
list(APPEND SOURCES src/notices.cpp ${TINTA_NOTICES_CPP})

set(HEADERS
    include/markdown.h
    include/types.h
    include/app.h
    include/settings.h
    include/keymap.h
    include/d2d_init.h
    include/syntax.h
    include/utils.h
    include/search.h
    include/search_panel.h
    include/render.h
    include/image_loader.h
    include/file_utils.h
    include/overlays.h
    include/input.h
    include/editor.h
    include/mermaid.h
    include/mermaid_ext.h
    include/document.h
    include/inline_style.h
    include/print.h
    include/i18n.h
    include/selection.h
    include/math_render.h
    include/plantuml.h
    include/plantuml_queue.h
    include/plantuml_app.h
    include/wordcount.h
)

# Linux build: everything platform-specific lives in platform/linux/
if(NOT WIN32)
    include(platform/linux/linux.cmake)
    return()
endif()

# Windows resource file (icon)
set(RESOURCES
    resources/tinta.rc
)

# Main executable
add_executable(tinta WIN32 ${SOURCES} ${HEADERS} ${RESOURCES})

target_include_directories(tinta PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${md4c_SOURCE_DIR}/src
)

target_link_libraries(tinta PRIVATE
    md4c
    d2d1
    dwrite
    shell32
    windowscodecs
    urlmon
    ole32
    imm32
    d3d11
    dwmapi
    comdlg32
    shlwapi
    winspool
)

target_compile_definitions(tinta PRIVATE UNICODE _UNICODE
    TINTA_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    TINTA_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    TINTA_VERSION_PATCH=${PROJECT_VERSION_PATCH})

# Mermaid through merman (docs/linux-port/PLAN.md §6, MM5): needs rustup and
# the x86_64-pc-windows-msvc target; drawn from merman's PNG on Windows.
# platform/windows/build-windows.sh turns it on for the cross build.
option(TINTA_MERMAN "Render Mermaid with merman (needs rustup)" OFF)
if(TINTA_MERMAN)
    include(cmake/merman.cmake)
    target_sources(tinta PRIVATE src/merman_engine.cpp)
    target_link_libraries(tinta PRIVATE tinta_merman)
endif()

if(BUILD_TESTING)
    add_executable(keymap_tests tests/keymap_tests.cpp src/keymap.cpp)
    target_include_directories(keymap_tests PRIVATE include)
    add_test(NAME keymap COMMAND keymap_tests)

    # Exercise the real input handlers and editor with a message-only test
    # window, including layout-dependent character sequences (#195).
    set(INPUT_TEST_SOURCES ${SOURCES})
    list(REMOVE_ITEM INPUT_TEST_SOURCES src/main_d2d.cpp)
    add_executable(input_tests tests/input_tests.cpp tests/table_input_tests.cpp tests/docked_preview_tests.cpp tests/table_layout_tests.cpp tests/window_drag_tests.cpp tests/tab_drop_tests.cpp tests/tab_hover_card_tests.cpp tests/search_panel_tests.cpp tests/superscript_tests.cpp tests/sidepanel_tests.cpp tests/image_tests.cpp tests/search_input_tests.cpp tests/wrapped_cursor_tests.cpp tests/file_fragment_tests.cpp tests/editor_context_tests.cpp tests/editor_features_tests.cpp tests/ui_font_tests.cpp tests/print_export_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(input_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(input_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(input_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_SHORTCUT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/shortcuts-layouts.md"
        TINTA_TAB_DROP_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tab-drop-position.md"
        TINTA_TAB_HOVER_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tab-hover-card"
        TINTA_SEARCH_PANEL_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/search-panel"
        TINTA_SUPERSCRIPT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/superscript-subscript.md"
        TINTA_CODE_BASELINE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inline-code-baseline-259.md"
        TINTA_PANEL_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/toc-browser-resize.md"
        TINTA_IMAGE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/image-cache-large.md"
        TINTA_FIND_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/find-table-focus.md"
        TINTA_WRAP_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/wrapped-cursor-selection.md"
        TINTA_SEARCH_WRAP_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/search-word-wrap-260.md"
        TINTA_ZOOM_WRAP_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/editor-zoom-wrap-266.md"
        TINTA_FRAGMENT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/file-fragments/index.md"
        TINTA_CONTEXT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/editor-context-selection.md"
        TINTA_MULTICURSOR_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/editor-multicursor-251.md"
        TINTA_PRINT_EXPORT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/print-word-export-256.md"
        TINTA_PRINT_EXPORT_THEMES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/print-word-export-themes.ini")
    add_test(NAME editor_features COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/editor-feature-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_editor_feature_tests.cmake)
    add_test(NAME ui_font_language COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/ui-font-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_ui_font_tests.cmake)
    add_test(NAME print_and_word_export COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/print-export-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_print_export_tests.cmake)
    add_test(NAME editor_context_selection COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/editor-context-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_editor_context_tests.cmake)
    add_test(NAME file_fragment_navigation COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/file-fragment-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_file_fragment_tests.cmake)
    add_test(NAME wrapped_cursor_navigation COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/wrapped-cursor-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_wrapped_cursor_tests.cmake)
    add_test(NAME search_input_focus COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/search-input-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_search_input_tests.cmake)
    add_test(NAME image_cache_and_decode COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/image-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_image_tests.cmake)
    add_test(NAME input_shortcuts COMMAND input_tests)
    add_test(NAME table_and_modifier_input COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/table-input-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_table_input_tests.cmake)
    add_test(NAME docked_preview_and_hints COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/docked-preview-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_docked_preview_tests.cmake)
    add_test(NAME wide_table_layout COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/table-layout-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_table_layout_tests.cmake)
    add_test(NAME window_drag_and_title_space COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/window-drag-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_window_drag_tests.cmake)
    add_test(NAME contents_and_panel_resize COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/sidepanel-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_sidepanel_tests.cmake)
    add_test(NAME superscript_and_learn COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/superscript-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_superscript_tests.cmake)
    add_test(NAME tab_drop_position COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/tab-drop-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tab_drop_tests.cmake)
    add_test(NAME tab_hover_card COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/tab-hover-card-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_tab_hover_card_tests.cmake)
    add_test(NAME search_results_panel COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:input_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/search-panel-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_search_panel_tests.cmake)

    add_executable(file_path_tests tests/file_path_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(file_path_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(file_path_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(file_path_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>)
    add_test(NAME copy_file_path COMMAND file_path_tests)

    add_executable(code_block_layout_tests tests/code_block_layout_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(code_block_layout_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(code_block_layout_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(code_block_layout_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_CODE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/code-block-spacing.md"
        TINTA_COPY_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/code-block-copy-269.md")
    add_test(NAME code_block_layout COMMAND code_block_layout_tests)

    add_executable(syntax_tests tests/syntax_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(syntax_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(syntax_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(syntax_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_SYNTAX_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME syntax_highlighting COMMAND syntax_tests)

    add_executable(frontmatter_tests tests/frontmatter_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(frontmatter_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(frontmatter_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(frontmatter_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_FRONTMATTER_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME frontmatter COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:frontmatter_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/frontmatter-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_portable_test.cmake)

    add_executable(footnote_tests tests/footnote_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(footnote_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(footnote_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(footnote_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_FOOTNOTE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/footnotes.md")
    add_test(NAME footnotes COMMAND footnote_tests)

    add_executable(html_span_tests tests/html_span_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(html_span_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(html_span_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(html_span_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_HTML_SPAN_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/html-inline-tags-263.md")
    add_test(NAME html_spans COMMAND html_span_tests)

    add_executable(admonition_tests tests/admonition_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(admonition_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(admonition_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(admonition_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_ADMONITION_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/admonitions.md")
    add_test(NAME admonitions COMMAND admonition_tests)

    add_executable(blockquote_layout_tests tests/blockquote_layout_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(blockquote_layout_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(blockquote_layout_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(blockquote_layout_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_QUOTE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/blockquote-spacing.md")
    add_test(NAME blockquote_layout COMMAND blockquote_layout_tests)

    add_executable(theme_tests tests/theme_tests.cpp tests/theme_colour_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(theme_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(theme_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(theme_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_THEME_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inline-code-color.md"
        TINTA_THEME_FONTS_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/theme-editor-fonts-265.md")
    add_test(NAME inline_code_theme COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:theme_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/theme-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_portable_test.cmake)

    add_executable(context_menu_tests tests/context_menu_tests.cpp src/context_menu.cpp
        src/markdown.cpp src/footnotes.cpp src/themes.cpp src/config_dir.cpp
        src/plantuml.cpp src/plantuml_queue.cpp)
    target_include_directories(context_menu_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(context_menu_tests PRIVATE md4c shell32)
    target_compile_definitions(context_menu_tests PRIVATE UNICODE _UNICODE)
    add_test(NAME context_menu COMMAND context_menu_tests)

    add_executable(math_parser_tests tests/math_parser_tests.cpp src/math_parser.cpp src/math_macros.cpp)
    target_include_directories(math_parser_tests PRIVATE src)
    add_test(NAME math_parser COMMAND math_parser_tests)

    add_executable(math_layout_tests tests/math_layout_tests.cpp
        src/math_parser.cpp src/math_macros.cpp src/math_render.cpp src/markdown.cpp src/footnotes.cpp src/themes.cpp src/config_dir.cpp
        src/plantuml.cpp src/plantuml_queue.cpp)
    target_include_directories(math_layout_tests PRIVATE include src ${md4c_SOURCE_DIR}/src)
    target_link_libraries(math_layout_tests PRIVATE md4c d2d1 dwrite windowscodecs shell32 ole32)
    target_compile_definitions(math_layout_tests PRIVATE UNICODE _UNICODE
        TINTA_MATH_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/math-compatibility.md")
    add_test(NAME math_layout COMMAND math_layout_tests)

    add_executable(mermaid_tests
        tests/mermaid_tests.cpp
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
    target_include_directories(mermaid_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    add_test(NAME mermaid_parser COMMAND mermaid_tests)

    add_executable(mermaid_fence_tests
        tests/mermaid_fence_tests.cpp
        src/mermaid.cpp src/mermaid_ext.cpp src/mermaid_seq.cpp src/mermaid_pie.cpp
        src/mermaid_uml.cpp src/mermaid_git.cpp src/mermaid_gantt.cpp
        src/mermaid_charts.cpp src/mermaid_narrative.cpp src/mermaid_grid.cpp
        src/mermaid_charts2.cpp src/mermaid_block.cpp src/mermaid_c4.cpp
        src/document.cpp src/frontmatter.cpp
        src/markdown.cpp src/footnotes.cpp
    )
    target_include_directories(mermaid_fence_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${md4c_SOURCE_DIR}/src
    )
    target_link_libraries(mermaid_fence_tests PRIVATE md4c)
    target_compile_definitions(mermaid_fence_tests PRIVATE UNICODE _UNICODE
        TINTA_MERMAID_PREAMBLE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mermaid-preamble.md")
    add_test(NAME mermaid_fences COMMAND mermaid_fence_tests)

    # PlantUML CLI test double (used by the plantuml suites; no Java needed).
    add_executable(fake_plantuml tests/fake_plantuml.cpp)

    # PlantUML core module unit suite: language gate, tool model, preamble
    # injection, cache key, command line and sync render, all driven against
    # the fake CLI double (no Java needed), plus the tool-discovery
    # precedence and the plantumlPath settings round-trip. Both tests run
    # through a portable driver: the suite exercises saveSettings/loadSettings
    # on a temp settings.ini and must never touch %APPDATA%\Tinta.
    add_executable(plantuml_tests tests/plantuml_tests.cpp src/plantuml.cpp
        src/settings.cpp src/i18n.cpp src/config_dir.cpp
        src/frontmatter.cpp src/keymap.cpp
        src/plantuml_queue.cpp)
    target_include_directories(plantuml_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
    target_link_libraries(plantuml_tests PRIVATE shell32)
    target_compile_definitions(plantuml_tests PRIVATE UNICODE _UNICODE
        TINTA_FAKE_PLANTUML="$<TARGET_FILE:fake_plantuml>"
        TINTA_PLANTUML_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/src/plantuml.cpp")
    add_dependencies(plantuml_tests fake_plantuml)
    add_test(NAME plantuml_parser COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:plantuml_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/plantuml-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_plantuml_tests.cmake)
    add_test(NAME plantuml_parser_selfcheck COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:plantuml_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/plantuml-selfcheck-test-$<CONFIG>
        -DTEST_ARGS=--expect-failure
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_plantuml_tests.cmake)
    set_tests_properties(plantuml_parser_selfcheck PROPERTIES WILL_FAIL TRUE)

    # PlantUML inside the app: layout, print preview, Word export, settings
    add_executable(plantuml_app_tests tests/plantuml_app_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(plantuml_app_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(plantuml_app_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(plantuml_app_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_FAKE_PLANTUML="$<TARGET_FILE:fake_plantuml>"
        TINTA_TRANSPARENT_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/plantuml-transparent")
    add_dependencies(plantuml_app_tests fake_plantuml)
    add_test(NAME plantuml_in_app COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:plantuml_app_tests>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/plantuml-app-test-$<CONFIG>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_portable_test.cmake)

    add_executable(document_tests
        tests/document_tests.cpp
        src/document.cpp src/frontmatter.cpp
        src/markdown.cpp src/footnotes.cpp
    )
    target_include_directories(document_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${md4c_SOURCE_DIR}/src
    )
    target_link_libraries(document_tests PRIVATE md4c)
    add_test(NAME document_types COMMAND document_tests)

    # Word and character counting (#240): plain text and parsed Markdown
    add_executable(wordcount_tests
        tests/wordcount_tests.cpp
        src/wordcount.cpp
        src/document.cpp src/frontmatter.cpp
        src/markdown.cpp src/footnotes.cpp
    )
    target_include_directories(wordcount_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${md4c_SOURCE_DIR}/src
    )
    target_link_libraries(wordcount_tests PRIVATE md4c)
    target_compile_definitions(wordcount_tests PRIVATE UNICODE _UNICODE
        TINTA_COUNT_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/word-count-240.md")
    add_test(NAME word_count COMMAND wordcount_tests)

    add_executable(inline_style_tests
        tests/inline_style_tests.cpp
        src/inline_style.cpp
        src/markdown.cpp src/footnotes.cpp
        src/themes.cpp
        src/config_dir.cpp
        src/plantuml.cpp src/plantuml_queue.cpp
    )
    target_include_directories(inline_style_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${md4c_SOURCE_DIR}/src
    )
    target_link_libraries(inline_style_tests PRIVATE md4c d2d1 dwrite windowscodecs shell32)
    target_compile_definitions(inline_style_tests PRIVATE UNICODE _UNICODE)
    add_test(NAME inline_styles COMMAND inline_style_tests)

    # Inline code on the text baseline, chips hugging the code (#262), at
    # 7/8 of the text around it, headings included (fork #1)
    add_executable(inline_code_baseline_tests tests/inline_code_baseline_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(inline_code_baseline_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(inline_code_baseline_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(inline_code_baseline_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>
        TINTA_INLINE_CODE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/inline-code-baseline-262.md"
        TINTA_HEADING_CODE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/heading-inline-code.md")
    add_test(NAME inline_code_baseline COMMAND inline_code_baseline_tests)

    # The default code face (Monaspace Argon) and its Consolas fallback
    add_executable(code_font_tests tests/code_font_tests.cpp ${INPUT_TEST_SOURCES})
    target_include_directories(code_font_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(code_font_tests PRIVATE $<TARGET_PROPERTY:tinta,LINK_LIBRARIES>)
    target_compile_definitions(code_font_tests PRIVATE $<TARGET_PROPERTY:tinta,COMPILE_DEFINITIONS>)
    add_test(NAME code_font COMMAND code_font_tests)

    add_executable(i18n_tests
        tests/i18n_tests.cpp
        src/i18n.cpp
        src/config_dir.cpp
    )
    target_include_directories(i18n_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(i18n_tests PRIVATE shell32)
    target_compile_definitions(i18n_tests PRIVATE UNICODE _UNICODE)
    add_test(NAME i18n COMMAND i18n_tests)

    add_executable(config_path_tests
        tests/config_path_tests.cpp
        src/config_dir.cpp
    )
    target_include_directories(config_path_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(config_path_tests PRIVATE shell32)
    target_compile_definitions(config_path_tests PRIVATE UNICODE _UNICODE)
    add_test(NAME config_paths COMMAND config_path_tests)
endif()

# Install
install(TARGETS tinta RUNTIME DESTINATION bin)
