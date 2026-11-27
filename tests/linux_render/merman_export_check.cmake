# MM3 exports: the four diagrams of mermaid-merman.md (all rejected by the
# native parser) reach the HTML export as Mermaid's own SVG (HTML labels in
# foreignObject) and the DOCX export as PNG images.
#   cmake -DTINTA=<tinta> -DDOC=<fixture> -DHOME_DIR=<scratch> -P merman_export_check.cmake
set(fail "")
file(REMOVE_RECURSE "${HOME_DIR}")
file(MAKE_DIRECTORY "${HOME_DIR}/config/Tinta" "${HOME_DIR}/data" "${HOME_DIR}/cache")
file(WRITE "${HOME_DIR}/config/Tinta/settings.ini"
    "checkUpdates=0\nhasAskedFileAssociation=1\nfollowSystemTheme=0\nthemeIndex=0\n")
set(env XDG_CONFIG_HOME=${HOME_DIR}/config XDG_DATA_HOME=${HOME_DIR}/data XDG_CACHE_HOME=${HOME_DIR}/cache)

execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} "${TINTA}" "${DOC}" --exporthtml "${HOME_DIR}/doc.html"
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT EXISTS "${HOME_DIR}/doc.html")
    string(APPEND fail "  HTML export failed (${rc})\n")
else()
    file(READ "${HOME_DIR}/doc.html" html)
    string(REGEX MATCHALL "<svg" svgs "${html}")
    list(LENGTH svgs count)
    if(NOT count EQUAL 4)
        string(APPEND fail "  HTML: ${count} inline SVGs, expected 4\n")
    endif()
    string(FIND "${html}" "<foreignObject" fo)
    if(fo EQUAL -1)
        string(APPEND fail "  HTML: no foreignObject labels (not Mermaid's parity SVG)\n")
    endif()
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E env ${env} "${TINTA}" "${DOC}" --exportdocx "${HOME_DIR}/doc.docx"
    RESULT_VARIABLE rc)
if(NOT rc EQUAL 0 OR NOT EXISTS "${HOME_DIR}/doc.docx")
    string(APPEND fail "  DOCX export failed (${rc})\n")
else()
    file(ARCHIVE_EXTRACT INPUT "${HOME_DIR}/doc.docx" DESTINATION "${HOME_DIR}/docx")
    file(GLOB images "${HOME_DIR}/docx/word/media/*.png")
    list(LENGTH images count)
    if(NOT count EQUAL 4)
        string(APPEND fail "  DOCX: ${count} PNG images, expected 4\n")
    endif()
endif()

if(fail)
    message(FATAL_ERROR "merman export check failed:\n${fail}")
endif()
message(STATUS "merman export check passed")
