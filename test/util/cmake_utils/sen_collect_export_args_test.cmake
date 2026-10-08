# === sen_collect_export_args_test.cmake ===============================================================================
#                                               Sen Infrastructure
#                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
#                                    See the LICENSE.txt file for more information.
#                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
# ======================================================================================================================

include(${CMAKE_CURRENT_LIST_DIR}/../../../cmake/util/sen_misc_utils.cmake)

function(get_target_property out_var)
  set(${out_var}
      "${out_var}-NOTFOUND"
      PARENT_SCOPE
  )
endfunction()

set(_source ${CMAKE_CURRENT_BINARY_DIR}/sen_collect_export_args_test_input.cpp)

function(expect_classes line expected)
  file(WRITE ${_source} "${line}\n")
  sen_collect_export_args(
    TARGET
    dummy
    EXPORT_NAME
    pkg
    SOURCES
    ${_source}
    EXPORT_CLASSES
    _actual
  )
  if(NOT
     "${_actual}"
     STREQUAL
     "${expected}"
  )
    message(SEND_ERROR "'${line}': expected '${expected}', got '${_actual}'")
  endif()
endfunction()

# valid names
expect_classes("SEN_EXPORT_CLASS(Plain)" "pkg.Plain")
expect_classes("SEN_EXPORT_CLASS(_Under9)" "pkg._Under9")

# surrounding whitespace is not part of the name
expect_classes("SEN_EXPORT_CLASS( Spaced )" "pkg.Spaced")
expect_classes("SEN_EXPORT_CLASS(\tTabbed\t)" "pkg.Tabbed")

# trailing and leading whitespaces is not part of the name
expect_classes(" \t  SEN_EXPORT_CLASS(Leading)" "pkg.Leading")
expect_classes("SEN_EXPORT_CLASS(Trailing)  \t" "pkg.Trailing")

# trailing text with parentheses is not part of the name
expect_classes("SEN_EXPORT_CLASS(Commented) // trailing comment (with parens)" "pkg.Commented")

# adding comments at the end is not part of the name
expect_classes("SEN_EXPORT_CLASS(Commented) /* trailing comment */" "pkg.Commented")
expect_classes("SEN_EXPORT_CLASS(Commented) /* trailing comment\n */" "pkg.Commented")

# adding semicolons inside the comments does not split the argument and is not part of the name
expect_classes("SEN_EXPORT_CLASS(SemiColon) // see also; bar" "pkg.SemiColon")
expect_classes("SEN_EXPORT_CLASS(SemiBlock) /* a; b */" "pkg.SemiBlock")

# unclosed bracket in a previous line comment
expect_classes(
  "//comment without ending bracket [\n SEN_EXPORT_CLASS(AfterOpenBracket)" "pkg.AfterOpenBracket"
)

# not a plain identifier, so nothing is exported
expect_classes("SEN_EXPORT_CLASS()" "")
expect_classes("SEN_EXPORT_CLASS(ns::Foo)" "")
expect_classes("SEN_EXPORT_CLASS(9Starts)" "")

# code after the macro on the same line, so nothing is exported
expect_classes("SEN_EXPORT_CLASS(TrailingCode) i++;" "")
expect_classes("SEN_EXPORT_CLASS(CommentThenCode)/* Comment */ i--;" "")

# no macro on the line
expect_classes("class Plain;" "")

file(REMOVE ${_source})
