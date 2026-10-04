# Script mode: generates Carbon/Text/Icons.h from the stylesheet of the Phosphor icon font.
#
#   cmake -DINPUT=<Phosphor/src/regular/style.css> -DOUTPUT=<Icons.h> -P GenerateIcons.cmake
#
# Each rule `.ph-address-book:before { content: "\e6f8"; }` becomes
#   inline constexpr const char* AddressBook = "\xee\x9b\xb8";   // the code point encoded as UTF-8
# The regular, bold and fill fonts share their code points, so one set of constants serves all three.

cmake_minimum_required(VERSION 3.25) # a script has no project to take its policies from

if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "GenerateIcons.cmake needs -DINPUT and -DOUTPUT")
endif()

file(READ "${INPUT}" CARBON_CSS)
string(REGEX MATCHALL "\\.ph-[a-z0-9-]+:before[ \t\r\n]*{[ \t\r\n]*content:[ \t]*\"\\\\[0-9a-f]+\"" CARBON_RULES "${CARBON_CSS}")

list(LENGTH CARBON_RULES CARBON_RULE_COUNT)
if(CARBON_RULE_COUNT EQUAL 0)
    message(FATAL_ERROR "GenerateIcons.cmake: no icon rules found in ${INPUT}")
endif()

set(CARBON_CONSTANTS "")
set(CARBON_TABLE "")
foreach(CARBON_RULE IN LISTS CARBON_RULES)
    string(REGEX MATCH "\\.ph-([a-z0-9-]+):before" CARBON_UNUSED "${CARBON_RULE}")
    set(CARBON_ICON_NAME "${CMAKE_MATCH_1}")
    string(REGEX MATCH "\\\\([0-9a-f]+)\"" CARBON_UNUSED "${CARBON_RULE}")
    set(CARBON_ICON_CODE "${CMAKE_MATCH_1}")

    # address-book -> AddressBook
    string(REPLACE "-" ";" CARBON_WORDS "${CARBON_ICON_NAME}")
    set(CARBON_IDENTIFIER "")
    foreach(CARBON_WORD IN LISTS CARBON_WORDS)
        string(SUBSTRING "${CARBON_WORD}" 0 1 CARBON_FIRST)
        string(SUBSTRING "${CARBON_WORD}" 1 -1 CARBON_REST)
        string(TOUPPER "${CARBON_FIRST}" CARBON_FIRST)
        string(APPEND CARBON_IDENTIFIER "${CARBON_FIRST}${CARBON_REST}")
    endforeach()

    # Encode the code point as UTF-8. Phosphor uses the Private Use Area (U+E000..U+F8FF): three bytes.
    math(EXPR CARBON_CODEPOINT "0x${CARBON_ICON_CODE}")
    if(CARBON_CODEPOINT LESS 2048 OR CARBON_CODEPOINT GREATER 65535)
        message(FATAL_ERROR "GenerateIcons.cmake: unexpected code point ${CARBON_ICON_CODE} for ${CARBON_ICON_NAME}")
    endif()
    math(EXPR CARBON_BYTE1 "224 | (${CARBON_CODEPOINT} >> 12)" OUTPUT_FORMAT HEXADECIMAL)
    math(EXPR CARBON_BYTE2 "128 | ((${CARBON_CODEPOINT} >> 6) & 63)" OUTPUT_FORMAT HEXADECIMAL)
    math(EXPR CARBON_BYTE3 "128 | (${CARBON_CODEPOINT} & 63)" OUTPUT_FORMAT HEXADECIMAL)
    string(SUBSTRING "${CARBON_BYTE1}" 2 -1 CARBON_BYTE1)
    string(SUBSTRING "${CARBON_BYTE2}" 2 -1 CARBON_BYTE2)
    string(SUBSTRING "${CARBON_BYTE3}" 2 -1 CARBON_BYTE3)
    set(CARBON_LITERAL "\"\\x${CARBON_BYTE1}\\x${CARBON_BYTE2}\\x${CARBON_BYTE3}\"")

    string(TOUPPER "${CARBON_ICON_CODE}" CARBON_ICON_CODE)
    string(APPEND CARBON_CONSTANTS
        "    /// Phosphor icon \"${CARBON_ICON_NAME}\" (U+${CARBON_ICON_CODE}).\n"
        "    inline constexpr const char* ${CARBON_IDENTIFIER} = ${CARBON_LITERAL};\n")
    string(APPEND CARBON_TABLE "        {\"${CARBON_IDENTIFIER}\", ${CARBON_IDENTIFIER}},\n")
endforeach()

file(WRITE "${OUTPUT}"
"#pragma once

// Generated from Phosphor's style.css by GenerateIcons.cmake. Do not edit.

#include <cstddef>

/// Icon constants for the embedded Phosphor icon font. Each one is a UTF-8 string holding a single code point, so
/// icons can be drawn with Carbon::Icon() or placed inside any text.
namespace Carbon::Icons
{
${CARBON_CONSTANTS}
    /// An icon's identifier and its string.
    struct Entry
    {
        const char* Name;
        const char* Glyph;
    };

    /// Number of icons.
    inline constexpr size_t Count = ${CARBON_RULE_COUNT};

    /// Every icon, sorted as in Phosphor's stylesheet. Useful for icon pickers and galleries.
    inline constexpr Entry All[Count] = {
${CARBON_TABLE}    };
} // namespace Carbon::Icons
")
