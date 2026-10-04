# Script mode: turns a binary file into a C++ source file that defines a byte array.
#
#   cmake -DINPUT=<file> -DOUTPUT=<file.cpp> -DSYMBOL=<Name> -P EmbedAsset.cmake
#
# The output defines, in namespace Carbon::Internal:
#   extern const unsigned char g_<Name>Data[];
#   extern const unsigned long long g_<Name>Size;
# C++20 has no #embed, so this runs at build time; the output lives in the build tree and is never committed.

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED SYMBOL)
    message(FATAL_ERROR "EmbedAsset.cmake needs -DINPUT, -DOUTPUT and -DSYMBOL")
endif()

file(READ "${INPUT}" CARBON_HEX HEX)
string(LENGTH "${CARBON_HEX}" CARBON_HEX_LENGTH)
math(EXPR CARBON_SIZE "${CARBON_HEX_LENGTH} / 2")

# "a1b2..." -> "0xa1,0xb2,..." with a line break every 24 bytes.
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," CARBON_BYTES "${CARBON_HEX}")
string(REGEX REPLACE "(0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],0x[0-9a-f][0-9a-f],)"
       "\\1\n    " CARBON_BYTES "${CARBON_BYTES}")

get_filename_component(CARBON_INPUT_NAME "${INPUT}" NAME)
file(WRITE "${OUTPUT}"
"// Generated from ${CARBON_INPUT_NAME} by EmbedAsset.cmake. Do not edit.
namespace Carbon::Internal
{
    extern const unsigned char g_${SYMBOL}Data[];
    extern const unsigned long long g_${SYMBOL}Size;

    const unsigned char g_${SYMBOL}Data[] = {
    ${CARBON_BYTES}
    };
    const unsigned long long g_${SYMBOL}Size = ${CARBON_SIZE}ull;
} // namespace Carbon::Internal
")
