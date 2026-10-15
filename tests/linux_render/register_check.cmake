# tinta --register on Linux, in private XDG folders: the desktop entry (Exec
# quoting the AppImage path), icons, the .mmd MIME type, Tinta as the
# default in mimeapps.list with other entries kept, and GIO agreeing.
#   cmake -DTINTA=<tinta> -DHOME_DIR=<scratch> -P register_check.cmake
set(fail "")
function(expect_text text needle message)  # `needle` occurs literally in `text`
    string(FIND "${text}" "${needle}" at)
    if(at EQUAL -1)
        set(fail "${fail}  ${message}\n" PARENT_SCOPE)
    endif()
endfunction()
function(expect_file path)
    if(NOT EXISTS "${path}")
        set(fail "${fail}  missing ${path}\n" PARENT_SCOPE)
    endif()
endfunction()

file(REMOVE_RECURSE "${HOME_DIR}")
file(MAKE_DIRECTORY "${HOME_DIR}/config/Tinta" "${HOME_DIR}/data" "${HOME_DIR}/cache" "${HOME_DIR}/Tinta App")
# GIO ignores entries whose program does not exist: a real executable, with a
# space in its path for the quoting
set(appimage "${HOME_DIR}/Tinta App/Tinta-x86_64.AppImage")
file(CREATE_LINK "${TINTA}" "${appimage}" SYMBOLIC)
file(WRITE "${HOME_DIR}/config/Tinta/settings.ini" "checkUpdates=0\nhasAskedFileAssociation=1\n")
file(WRITE "${HOME_DIR}/config/mimeapps.list"
    "# the user's own\n[Default Applications]\nimage/png=org.kde.gwenview.desktop\n")
set(env XDG_CONFIG_HOME=${HOME_DIR}/config XDG_DATA_HOME=${HOME_DIR}/data XDG_CACHE_HOME=${HOME_DIR}/cache
    "APPIMAGE=${appimage}")
execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} "${TINTA}" --register
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
    string(APPEND fail "  exit code ${rc}\n")
endif()
expect_text("${err}" "is now the default viewer for .md" "no Linux confirmation on stderr: ${err}")

set(entry "${HOME_DIR}/data/applications/tinta.desktop")
expect_file("${entry}")
if(EXISTS "${entry}")
    file(READ "${entry}" text)
    expect_text("${text}" "\nExec=\"${appimage}\" %f\n" "Exec line:\n${text}")
    expect_text("${text}" "\nMimeType=text/markdown;text/x-markdown;text/vnd.mermaid;\n" "MimeType line")
endif()
foreach(size 16 32 48 256)
    expect_file("${HOME_DIR}/data/icons/hicolor/${size}x${size}/apps/tinta.png")
endforeach()
expect_file("${HOME_DIR}/data/mime/packages/tinta-mermaid.xml")

file(READ "${HOME_DIR}/config/mimeapps.list" apps)
foreach(type text/markdown text/x-markdown text/vnd.mermaid)
    expect_text("${apps}" "\n${type}=tinta.desktop;" "${type} default missing:\n${apps}")
endforeach()
expect_text("${apps}" "image/png=org.kde.gwenview.desktop" "an existing default was lost")
expect_text("${apps}" "# the user's own" "a comment was lost")

# A real consumer: GIO resolves the default from these folders
find_program(GIO gio)
if(GIO)
    execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} ${GIO} mime text/markdown
        OUTPUT_VARIABLE gio ERROR_VARIABLE gio_err)
    expect_text("${gio}" "tinta.desktop" "gio mime text/markdown: ${gio}${gio_err}")
endif()

if(fail)
    message(FATAL_ERROR "register check failed:\n${fail}")
endif()
message(STATUS "register check passed (gio: ${GIO})")
