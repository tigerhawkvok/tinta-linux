# Upstream's native test suites (CMakeLists.txt, Windows block) on the Linux
# compat layer: the same test sources, fixtures and run_*.cmake drivers, with
# tinta_core in place of the Windows source list. Keep in step with upstream.

set(TINTA_FIXTURES ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures)

function(tinta_upstream_test target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE tinta_core)
endfunction()

# A test run through one of upstream's run_*.cmake drivers
function(tinta_driver_test name binary dir script)
    add_test(NAME ${name} COMMAND ${CMAKE_COMMAND}
        -DTEST_BINARY=$<TARGET_FILE:${binary}>
        -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/${dir}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/${script})
endfunction()

tinta_upstream_test(input_tests tests/input_tests.cpp tests/table_input_tests.cpp tests/docked_preview_tests.cpp
    tests/table_layout_tests.cpp tests/window_drag_tests.cpp tests/tab_drop_tests.cpp tests/tab_hover_card_tests.cpp
    tests/search_panel_tests.cpp tests/superscript_tests.cpp tests/sidepanel_tests.cpp tests/image_tests.cpp
    tests/search_input_tests.cpp tests/wrapped_cursor_tests.cpp tests/file_fragment_tests.cpp
    tests/editor_context_tests.cpp tests/editor_features_tests.cpp tests/ui_font_tests.cpp
    tests/print_export_tests.cpp)
target_compile_definitions(input_tests PRIVATE
    TINTA_SHORTCUT_FIXTURE="${TINTA_FIXTURES}/shortcuts-layouts.md"
    TINTA_TAB_DROP_FIXTURE="${TINTA_FIXTURES}/tab-drop-position.md"
    TINTA_TAB_HOVER_FIXTURE_DIR="${TINTA_FIXTURES}/tab-hover-card"
    TINTA_SEARCH_PANEL_FIXTURE_DIR="${TINTA_FIXTURES}/search-panel"
    TINTA_SUPERSCRIPT_FIXTURE="${TINTA_FIXTURES}/superscript-subscript.md"
    TINTA_CODE_BASELINE_FIXTURE="${TINTA_FIXTURES}/inline-code-baseline-259.md"
    TINTA_PANEL_FIXTURE="${TINTA_FIXTURES}/toc-browser-resize.md"
    TINTA_IMAGE_FIXTURE="${TINTA_FIXTURES}/image-cache-large.md"
    TINTA_FIND_FIXTURE="${TINTA_FIXTURES}/find-table-focus.md"
    TINTA_WRAP_FIXTURE="${TINTA_FIXTURES}/wrapped-cursor-selection.md"
    TINTA_SEARCH_WRAP_FIXTURE="${TINTA_FIXTURES}/search-word-wrap-260.md"
    TINTA_ZOOM_WRAP_FIXTURE="${TINTA_FIXTURES}/editor-zoom-wrap-266.md"
    TINTA_FRAGMENT_FIXTURE="${TINTA_FIXTURES}/file-fragments/index.md"
    TINTA_CONTEXT_FIXTURE="${TINTA_FIXTURES}/editor-context-selection.md"
    TINTA_MULTICURSOR_FIXTURE="${TINTA_FIXTURES}/editor-multicursor-251.md"
    TINTA_PRINT_EXPORT_FIXTURE="${TINTA_FIXTURES}/print-word-export-256.md"
    TINTA_PRINT_EXPORT_THEMES="${TINTA_FIXTURES}/print-word-export-themes.ini")
tinta_driver_test(editor_features input_tests editor-feature-test run_editor_feature_tests.cmake)
tinta_driver_test(ui_font_language input_tests ui-font-test run_ui_font_tests.cmake)
tinta_driver_test(print_and_word_export input_tests print-export-test run_print_export_tests.cmake)
tinta_driver_test(editor_context_selection input_tests editor-context-test run_editor_context_tests.cmake)
tinta_driver_test(file_fragment_navigation input_tests file-fragment-test run_file_fragment_tests.cmake)
tinta_driver_test(wrapped_cursor_navigation input_tests wrapped-cursor-test run_wrapped_cursor_tests.cmake)
tinta_driver_test(search_input_focus input_tests search-input-test run_search_input_tests.cmake)
tinta_driver_test(image_cache_and_decode input_tests image-test run_image_tests.cmake)
add_test(NAME input_shortcuts COMMAND input_tests)
tinta_driver_test(table_and_modifier_input input_tests table-input-test run_table_input_tests.cmake)
tinta_driver_test(docked_preview_and_hints input_tests docked-preview-test run_docked_preview_tests.cmake)
tinta_driver_test(wide_table_layout input_tests table-layout-test run_table_layout_tests.cmake)
tinta_driver_test(window_drag_and_title_space input_tests window-drag-test run_window_drag_tests.cmake)
tinta_driver_test(contents_and_panel_resize input_tests sidepanel-test run_sidepanel_tests.cmake)
tinta_driver_test(superscript_and_learn input_tests superscript-test run_superscript_tests.cmake)
tinta_driver_test(tab_drop_position input_tests tab-drop-test run_tab_drop_tests.cmake)
tinta_driver_test(tab_hover_card input_tests tab-hover-card-test run_tab_hover_card_tests.cmake)
tinta_driver_test(search_results_panel input_tests search-panel-test run_search_panel_tests.cmake)

tinta_upstream_test(file_path_tests tests/file_path_tests.cpp)
add_test(NAME copy_file_path COMMAND file_path_tests)

tinta_upstream_test(code_block_layout_tests tests/code_block_layout_tests.cpp)
target_compile_definitions(code_block_layout_tests PRIVATE TINTA_CODE_FIXTURE="${TINTA_FIXTURES}/code-block-spacing.md"
    TINTA_COPY_FIXTURE="${TINTA_FIXTURES}/code-block-copy-269.md")
add_test(NAME code_block_layout COMMAND code_block_layout_tests)

tinta_upstream_test(syntax_tests tests/syntax_tests.cpp)
target_compile_definitions(syntax_tests PRIVATE TINTA_SYNTAX_FIXTURES="${TINTA_FIXTURES}")
add_test(NAME syntax_highlighting COMMAND syntax_tests)

tinta_upstream_test(frontmatter_tests tests/frontmatter_tests.cpp)
target_compile_definitions(frontmatter_tests PRIVATE TINTA_FRONTMATTER_FIXTURES="${TINTA_FIXTURES}")
tinta_driver_test(frontmatter frontmatter_tests frontmatter-test run_portable_test.cmake)

tinta_upstream_test(footnote_tests tests/footnote_tests.cpp)
target_compile_definitions(footnote_tests PRIVATE TINTA_FOOTNOTE_FIXTURE="${TINTA_FIXTURES}/footnotes.md")
add_test(NAME footnotes COMMAND footnote_tests)

tinta_upstream_test(html_span_tests tests/html_span_tests.cpp)
target_compile_definitions(html_span_tests PRIVATE TINTA_HTML_SPAN_FIXTURE="${TINTA_FIXTURES}/html-inline-tags-263.md")
add_test(NAME html_spans COMMAND html_span_tests)

tinta_upstream_test(blockquote_layout_tests tests/blockquote_layout_tests.cpp)
target_compile_definitions(blockquote_layout_tests PRIVATE TINTA_QUOTE_FIXTURE="${TINTA_FIXTURES}/blockquote-spacing.md")
add_test(NAME blockquote_layout COMMAND blockquote_layout_tests)

tinta_upstream_test(theme_tests tests/theme_tests.cpp tests/theme_colour_tests.cpp)
target_compile_definitions(theme_tests PRIVATE TINTA_THEME_FIXTURE="${TINTA_FIXTURES}/inline-code-color.md"
    TINTA_THEME_FONTS_FIXTURE="${TINTA_FIXTURES}/theme-editor-fonts-265.md")
tinta_driver_test(inline_code_theme theme_tests theme-test run_portable_test.cmake)

tinta_upstream_test(context_menu_tests tests/context_menu_tests.cpp)
add_test(NAME context_menu COMMAND context_menu_tests)

tinta_upstream_test(math_layout_tests tests/math_layout_tests.cpp)
target_include_directories(math_layout_tests PRIVATE src)
target_compile_definitions(math_layout_tests PRIVATE TINTA_MATH_FIXTURE="${TINTA_FIXTURES}/math-compatibility.md")
add_test(NAME math_layout COMMAND math_layout_tests)

tinta_upstream_test(inline_style_tests tests/inline_style_tests.cpp)
add_test(NAME inline_styles COMMAND inline_style_tests)

tinta_upstream_test(i18n_tests tests/i18n_tests.cpp)
add_test(NAME i18n COMMAND i18n_tests)

tinta_upstream_test(config_path_tests tests/config_path_tests.cpp)
add_test(NAME config_paths COMMAND config_path_tests)


tinta_upstream_test(inline_code_baseline_tests tests/inline_code_baseline_tests.cpp)
target_compile_definitions(inline_code_baseline_tests PRIVATE
    TINTA_INLINE_CODE_FIXTURE="${TINTA_FIXTURES}/inline-code-baseline-262.md")
add_test(NAME inline_code_baseline COMMAND inline_code_baseline_tests)

# The default code face (Monaspace Argon) and its fallback
tinta_upstream_test(code_font_tests tests/code_font_tests.cpp)
add_test(NAME code_font COMMAND code_font_tests)

# PlantUML (#248): the CLI test double (no Java needed) and the core suite,
# through upstream's portable driver; the self-check must fail
add_executable(fake_plantuml tests/fake_plantuml.cpp)
tinta_upstream_test(plantuml_tests tests/plantuml_tests.cpp)
target_compile_definitions(plantuml_tests PRIVATE
    TINTA_FAKE_PLANTUML="$<TARGET_FILE:fake_plantuml>"
    TINTA_PLANTUML_SOURCE="${CMAKE_CURRENT_SOURCE_DIR}/src/plantuml.cpp")
add_dependencies(plantuml_tests fake_plantuml)
tinta_driver_test(plantuml_parser plantuml_tests plantuml-test run_plantuml_tests.cmake)
add_test(NAME plantuml_parser_selfcheck COMMAND ${CMAKE_COMMAND}
    -DTEST_BINARY=$<TARGET_FILE:plantuml_tests>
    -DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/plantuml-selfcheck-test
    -DTEST_ARGS=--expect-failure
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_plantuml_tests.cmake)
set_tests_properties(plantuml_parser_selfcheck PROPERTIES WILL_FAIL TRUE)

# PlantUML inside the app: layout, print preview, Word export, settings
tinta_upstream_test(plantuml_app_tests tests/plantuml_app_tests.cpp)
target_compile_definitions(plantuml_app_tests PRIVATE
    TINTA_FAKE_PLANTUML="$<TARGET_FILE:fake_plantuml>"
    TINTA_TRANSPARENT_FIXTURE_DIR="${TINTA_FIXTURES}/plantuml-transparent")
add_dependencies(plantuml_app_tests fake_plantuml)
tinta_driver_test(plantuml_in_app plantuml_app_tests plantuml-app-test run_portable_test.cmake)

# Admonitions in every dialect: layout, folding, print and the exports
tinta_upstream_test(admonition_tests tests/admonition_tests.cpp)
target_compile_definitions(admonition_tests PRIVATE TINTA_ADMONITION_FIXTURE="${TINTA_FIXTURES}/admonitions.md")
add_test(NAME admonitions COMMAND admonition_tests)
