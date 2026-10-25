# Turns a file into a C++ header with its bytes, so the web panel ships inside the plugin.
# Usage: cmake -DINPUT=<file> -DOUTPUT=<header> -DSYMBOL=<name> -P EmbedFile.cmake
file(READ "${INPUT}" content HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${content}")
file(WRITE "${OUTPUT}"
	"#pragma once\n"
	"#include <cstddef>\n"
	"// Generated from ${INPUT}, do not edit.\n"
	"namespace sorteador\n{\n"
	"static const unsigned char ${SYMBOL}[] = { ${bytes} };\n"
	"static const size_t ${SYMBOL}Size = sizeof( ${SYMBOL} );\n"
	"}\n")
