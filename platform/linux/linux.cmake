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
# The window layer (gtk_window.cpp); headless runs never initialise it
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4>=4.10)
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
    ${TINTA_LINUX_DIR}/src/gtk_window.cpp
    ${TINTA_LINUX_DIR}/src/win32_system.cpp
    ${TINTA_LINUX_DIR}/src/desktop_integration.cpp
)
target_include_directories(tinta_compat PUBLIC ${TINTA_LINUX_DIR}/include)
target_compile_definitions(tinta_compat PUBLIC UNICODE _UNICODE NOMINMAX)
target_link_libraries(tinta_compat PUBLIC PkgConfig::GLIB PkgConfig::PANGOCAIRO PkgConfig::CAIRO
    PkgConfig::PIXBUF PkgConfig::RSVG PkgConfig::FONTCONFIG PkgConfig::GTK4)
# Bundled fonts (Selawik for Segoe UI): found beside an installed binary in
# share/tinta/fonts, or in the source tree when run from a build
target_compile_definitions(tinta_compat PRIVATE
    TINTA_SOURCE_FONT_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts"
    TINTA_SOURCE_ICON_DIR="${TINTA_LINUX_DIR}/data/icons")
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts/ DESTINATION share/tinta/fonts COMPONENT tinta)
# Tinta's license and everything it ships under (also built into the binary)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/packaging/THIRD-PARTY-NOTICES.md
        DESTINATION share/doc/tinta COMPONENT tinta)
# Desktop integration; the icons come from resources/tinta.ico. The desktop
# file name matches the GTK program name (Wayland app id, WM_CLASS)
install(FILES ${TINTA_LINUX_DIR}/data/tinta.desktop DESTINATION share/applications COMPONENT tinta)
foreach(size 16 32 48 256)
    install(FILES ${TINTA_LINUX_DIR}/data/icons/tinta-${size}.png
        DESTINATION share/icons/hicolor/${size}x${size}/apps RENAME tinta.png COMPONENT tinta)
endforeach()
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

# The app: Tinta's own WinMain behind a Linux main(), in a GTK window, or
# headless with --render-png, --printpages and --export*.
add_executable(tinta ${TINTA_LINUX_DIR}/src/main_linux.cpp src/main_d2d.cpp)
target_include_directories(tinta PRIVATE ${TINTA_LINUX_DIR}/src)
target_link_libraries(tinta PRIVATE tinta_core)
install(TARGETS tinta RUNTIME DESTINATION bin COMPONENT tinta)

# Mermaid through merman instead of the native parsers (PLAN.md §6)
option(TINTA_MERMAN "Render Mermaid with merman (needs rustup)" ON)
if(TINTA_MERMAN)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/merman.cmake)
    # The viewer draws Mermaid through merman (render.cpp); the native
    # renderer stays linked as its fallback
    target_sources(tinta_core PRIVATE src/merman_engine.cpp)
    target_link_libraries(tinta_core PUBLIC tinta_merman)
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

    add_executable(compat_crt_tests ${TINTA_LINUX_DIR}/tests/crt_tests.cpp)
    add_test(NAME compat_crt COMMAND compat_crt_tests)

    add_executable(compat_d2d_wic_tests ${TINTA_LINUX_DIR}/tests/d2d_wic_tests.cpp)
    target_include_directories(compat_d2d_wic_tests PRIVATE ${TINTA_LINUX_DIR}/src)
    find_package(ZLIB REQUIRED)  # reads the compressed PDF the print path writes
    target_link_libraries(compat_d2d_wic_tests PRIVATE tinta_compat ZLIB::ZLIB)
    add_test(NAME compat_d2d_wic COMMAND compat_d2d_wic_tests)

    # The whole app headless: an absolute path argument opens the document
    # (it is not taken for a /switch) and the window paints and saves
    set(TINTA_SMOKE_HOME ${CMAKE_BINARY_DIR}/linux-render/.ctest-home)
    file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/linux-render)
    add_test(NAME linux_headless_render
        COMMAND tinta ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/markdown-regression-control.md
            --render-png=${CMAKE_BINARY_DIR}/linux-render/ctest-control.png --size=900x700 --settle-ms=100)
    set_tests_properties(linux_headless_render PROPERTIES
        ENVIRONMENT "XDG_CONFIG_HOME=${TINTA_SMOKE_HOME}/config;XDG_CACHE_HOME=${TINTA_SMOKE_HOME}/cache;XDG_DATA_HOME=${TINTA_SMOKE_HOME}/data"
        PASS_REGULAR_EXPRESSION "render-png: window \"[^\"]*markdown-regression-control\\.md"
        # A pass regex overrides the exit code, so a failed PNG write must fail here
        FAIL_REGULAR_EXPRESSION "no painted window|\\[timeout\\]|render-png: [^\n]*\\.png: ")

    # Tinta's own PDF export through the print path (pages are released
    # before the print control closes, which once aborted in cairo-pdf)
    add_test(NAME linux_headless_pdf
        COMMAND tinta ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/math-mixed-layout.md
            --exportpdf ${CMAKE_BINARY_DIR}/linux-render/ctest-math.pdf)
    set_tests_properties(linux_headless_pdf PROPERTIES
        ENVIRONMENT "XDG_CONFIG_HOME=${TINTA_SMOKE_HOME}/config;XDG_CACHE_HOME=${TINTA_SMOKE_HOME}/cache;XDG_DATA_HOME=${TINTA_SMOKE_HOME}/data")

    # --register: desktop entry, icons, .mmd MIME type and mimeapps.list,
    # all in private XDG folders
    add_test(NAME linux_register COMMAND ${CMAKE_COMMAND} -DTINTA=$<TARGET_FILE:tinta>
        -DHOME_DIR=${CMAKE_BINARY_DIR}/linux-render/.register-home
        -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_render/register_check.cmake)

    # The GTK window driven with real X input on a private Xvfb server
    # (tests/linux_render/gui_check.py): needs xvfb-run and a Python with Pillow
    set(TINTA_GUI_PYTHON "" CACHE FILEPATH "Python with Pillow for the GTK window check (empty: skip it)")
    find_program(TINTA_XVFB_RUN xvfb-run)
    if(TINTA_GUI_PYTHON AND TINTA_XVFB_RUN)
        add_test(NAME linux_gtk_window
            COMMAND ${TINTA_GUI_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_render/gui_check.py
                $<TARGET_FILE:tinta> ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/gtk-window.md
                --out ${CMAKE_BINARY_DIR}/linux-render/gui-check)
        set_tests_properties(linux_gtk_window PROPERTIES TIMEOUT 120)
        # Tabs dragged between windows and torn off, through the desktop's
        # drag and drop (private D-Bus session, Xvfb)
        find_program(TINTA_DBUS_RUN_SESSION dbus-run-session)
        if(TINTA_DBUS_RUN_SESSION)
            add_test(NAME linux_tab_drag
                COMMAND ${TINTA_GUI_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_render/tab_drag_check.py
                    $<TARGET_FILE:tinta> --out ${CMAKE_BINARY_DIR}/linux-render/tab-drag)
            set_tests_properties(linux_tab_drag PROPERTIES TIMEOUT 180)
            # Print: the GTK dialog prints the exported PDF (Print to File)
            add_test(NAME linux_print
                COMMAND ${TINTA_GUI_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_render/print_check.py
                    $<TARGET_FILE:tinta> ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/paste-image.md
                    --out ${CMAKE_BINARY_DIR}/linux-render/print)
            set_tests_properties(linux_print PROPERTIES TIMEOUT 180)
        endif()
    endif()

    # Win32 clipboard -> GdkClipboard (text, and PNG as an image), on Xvfb
    add_executable(clipboard_bridge_tests ${TINTA_LINUX_DIR}/tests/clipboard_bridge_tests.cpp)
    target_include_directories(clipboard_bridge_tests PRIVATE ${TINTA_LINUX_DIR}/src)
    target_link_libraries(clipboard_bridge_tests PRIVATE tinta_compat)
    if(TINTA_XVFB_RUN)
        add_test(NAME compat_clipboard_bridge
            COMMAND ${TINTA_XVFB_RUN} -a $<TARGET_FILE:clipboard_bridge_tests>)
        # X11 only: the user's own Wayland session must never be touched
        set_tests_properties(compat_clipboard_bridge PROPERTIES
            ENVIRONMENT "GDK_BACKEND=x11;WAYLAND_DISPLAY=;GSK_RENDERER=cairo" TIMEOUT 60)
    endif()

    # List items stand apart (render.cpp layoutList)
    add_executable(list_spacing_tests tests/list_spacing_tests.cpp)
    target_include_directories(list_spacing_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(list_spacing_tests PRIVATE tinta_core)
    add_test(NAME list_spacing COMMAND list_spacing_tests)

    # The third-party notices: built in, complete, and opened as a tab
    add_executable(notices_tests tests/notices_tests.cpp)
    target_include_directories(notices_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(notices_tests PRIVATE tinta_core)
    target_compile_definitions(notices_tests PRIVATE
        TINTA_NOTICES_FILE="${CMAKE_CURRENT_SOURCE_DIR}/packaging/THIRD-PARTY-NOTICES.md")
    add_test(NAME notices COMMAND notices_tests)
    # ...and current: regenerated from the pinned merman, they must not differ
    find_program(TINTA_PYTHON3 python3)
    if(TINTA_MERMAN AND TINTA_PYTHON3)
        add_test(NAME notices_current COMMAND ${TINTA_PYTHON3}
            ${CMAKE_CURRENT_SOURCE_DIR}/packaging/notices/generate_notices.py --check
            --merman ${merman_SOURCE_DIR} --md4c ${md4c_SOURCE_DIR}
            --out ${CMAKE_CURRENT_SOURCE_DIR}/packaging/THIRD-PARTY-NOTICES.md)
    endif()

    # Copying from the reader returns Markdown (markdown_copy.cpp)
    add_executable(copy_markdown_tests tests/copy_markdown_tests.cpp)
    target_include_directories(copy_markdown_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(copy_markdown_tests PRIVATE tinta_core)
    target_compile_definitions(copy_markdown_tests PRIVATE
        TINTA_COPY_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/copy-as-markdown.md")
    add_test(NAME copy_markdown COMMAND copy_markdown_tests)

    # The Emacs shortcut profile (emacs_keys.cpp), reader and editor
    add_executable(emacs_keys_tests tests/emacs_keys_tests.cpp)
    target_include_directories(emacs_keys_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(emacs_keys_tests PRIVATE tinta_core)
    target_compile_definitions(emacs_keys_tests PRIVATE
        TINTA_EMACS_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/emacs-keys.md")
    add_test(NAME emacs_keys COMMAND emacs_keys_tests)

    # Pasting an image into the editor: the card, assets, inline, off
    add_executable(paste_image_tests tests/paste_image_tests.cpp)
    target_include_directories(paste_image_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
    target_link_libraries(paste_image_tests PRIVATE tinta_core)
    target_compile_definitions(paste_image_tests PRIVATE
        TINTA_PASTE_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/paste-image.md")
    add_test(NAME paste_image COMMAND paste_image_tests)

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

        # MM3: the viewer draws fences through merman (render.cpp)
        add_executable(merman_layout_tests tests/merman_layout_tests.cpp)
        target_include_directories(merman_layout_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
        target_link_libraries(merman_layout_tests PRIVATE tinta_core)
        target_compile_definitions(merman_layout_tests PRIVATE
            TINTA_MERMAN_FIXTURE="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mermaid-merman.md")
        add_test(NAME merman_layout COMMAND merman_layout_tests)

        # MM2: shapes sized from labels measured in the face that draws them
        add_executable(merman_label_width_tests tests/merman_label_width_tests.cpp)
        target_include_directories(merman_label_width_tests PRIVATE include ${md4c_SOURCE_DIR}/src)
        target_link_libraries(merman_label_width_tests PRIVATE tinta_core)
        add_test(NAME merman_label_widths COMMAND merman_label_width_tests)
        add_test(NAME linux_merman_exports COMMAND ${CMAKE_COMMAND} -DTINTA=$<TARGET_FILE:tinta>
            -DDOC=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mermaid-merman.md
            -DHOME_DIR=${CMAKE_BINARY_DIR}/linux-render/.merman-exports
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/linux_render/merman_export_check.cmake)

        # merman's raster (the Windows path) loads the fonts Tinta names
        if(TINTA_MERMAN_PNG)
            add_executable(merman_raster_fonts_tests tests/merman_raster_fonts_tests.cpp src/merman_engine.cpp)
            target_include_directories(merman_raster_fonts_tests PRIVATE include)
            target_link_libraries(merman_raster_fonts_tests PRIVATE tinta_compat tinta_merman)
            target_compile_definitions(merman_raster_fonts_tests PRIVATE
                TINTA_FONTS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/third_party/fonts")
            add_test(NAME merman_raster_fonts COMMAND merman_raster_fonts_tests)
        endif()
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
