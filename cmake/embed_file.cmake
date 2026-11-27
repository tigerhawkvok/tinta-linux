# Writes OUTPUT, a C++ file defining SYMBOL (the bytes of INPUT, with a
# trailing NUL) and SYMBOLSize (the length without it).
#   cmake -DINPUT=file -DOUTPUT=file.cpp -DSYMBOL=kName -P embed_file.cmake
file(READ "${INPUT}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," hex "${hex}")
string(REGEX REPLACE "((0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],)(0x[0-9a-f][0-9a-f],))"
       "\\1\n" hex "${hex}")
file(WRITE "${OUTPUT}"
     "// Generated from ${INPUT} by cmake/embed_file.cmake\n"
     "#include <cstddef>\n"
     "extern const unsigned char ${SYMBOL}[] = {\n${hex}0x00};\n"
     "extern const std::size_t ${SYMBOL}Size = sizeof(${SYMBOL}) - 1;\n")
