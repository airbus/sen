// === stl_token.h =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_TOKEN_H
#define SEN_CORE_LANG_STL_TOKEN_H

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/lang/code_location.h"
#include "sen/core/meta/var.h"

// std
#include <string>
#include <vector>

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Classification of every lexeme produced by `StlScanner`.
enum class StlTokenType
{
  // ---- single-character punctuation ----
  leftParen,     ///< `(`
  rightParen,    ///< `)`
  leftBrace,     ///< `{`
  rightBrace,    ///< `}`
  leftBracket,   ///< `[`
  rightBracket,  ///< `]`
  comma,         ///< `,`
  dot,           ///< `.`
  minus,         ///< `-`
  plus,          ///< `+`
  colon,         ///< `:`
  semicolon,     ///< `;`
  slash,         ///< `/`
  star,          ///< `*`
  at,            ///< `@`

  // ---- one-or-two-character operators ----
  bang,          ///< `!`
  bangEqual,     ///< `!=`
  equal,         ///< `=`
  equalEqual,    ///< `==`
  greater,       ///< `>`
  greaterEqual,  ///< `>=`
  less,          ///< `<`
  lessEqual,     ///< `<=`

  // ---- literals ----
  identifier,  ///< A user-defined name (package, type, member, variable).
  string,      ///< A quoted string literal.
  real,        ///< A floating-point numeric literal.
  integral,    ///< An integer numeric literal.

  // ---- STL type-system keywords ----
  keywordAbstract,    ///< `abstract`
  keywordAnd,         ///< `and`
  keywordClass,       ///< `class`
  keywordInterface,   ///< `interface`
  keywordExtends,     ///< `extends`
  keywordImplements,  ///< `implements`
  keywordStruct,      ///< `struct`
  keywordVariant,     ///< `variant`
  keywordFalse,       ///< `false`
  keywordFunction,    ///< `fn`
  keywordEvent,       ///< `event`
  keywordVar,         ///< `var`
  keywordOr,          ///< `or`
  keywordNot,         ///< `not`
  keywordTrue,        ///< `true`
  keywordImport,      ///< `import`
  keywordOptional,    ///< `optional`
  keywordPackage,     ///< `package`
  keywordEnum,        ///< `enum`
  keywordSequence,    ///< `sequence`
  keywordArray,       ///< `array`
  keywordQuantity,    ///< `quantity`
  keywordAlias,       ///< `alias`

  // ---- Sen Query Language keywords ----
  keywordSelect,   ///< `SELECT`
  keywordWhere,    ///< `WHERE`
  keywordFrom,     ///< `FROM`
  keywordIn,       ///< `IN`
  keywordBetween,  ///< `BETWEEN`

  // ---- meta tokens ----
  comment,    ///< A doc-comment block attached to the next declaration.
  endOfFile,  ///< Marks the end of the token stream; always the last token.
};

/// A single lexical unit produced by `StlScanner`.
/// Carries the token classification, the raw source text, an optional parsed literal value,
/// and the location in the source for error reporting.
struct StlToken
{
  SEN_COPY_CONSTRUCT(StlToken) = default;
  SEN_COPY_ASSIGN(StlToken) = default;
  SEN_MOVE_CONSTRUCT(StlToken) = default;
  SEN_MOVE_ASSIGN(StlToken) = default;

  StlToken() = default;
  /// @param type   Classification of this token.
  /// @param lexeme Raw source text that produced this token.
  /// @param value  Parsed literal value (populated for `real`, `integral`, and `string` tokens).
  /// @param loc    Source location, used for error messages.
  StlToken(StlTokenType type, std::string lexeme, Var value, CodeLocation loc)
    : type(std::move(type)), lexeme(std::move(lexeme)), value(std::move(value)), loc(std::move(loc))
  {
  }
  ~StlToken() = default;

  StlTokenType type = {};   ///< Classification of this token. NOLINT(misc-non-private-member-variables-in-classes)
  std::string lexeme = {};  ///< Raw source text exactly as it appeared in the STL source.
                            ///< NOLINT(misc-non-private-member-variables-in-classes)
  Var value =
    {};  ///< Parsed literal value; empty for non-literal tokens. NOLINT(misc-non-private-member-variables-in-classes)
  CodeLocation loc =
    {};  ///< Position in the source string for error reporting. NOLINT(misc-non-private-member-variables-in-classes)
};

/// Formats a token as `<type> '<lexeme>'` for diagnostic output.
/// @param token The token to format.
/// @return Human-readable string describing the token.
std::string toString(const StlToken& token);

/// Returns the canonical name of a token type (e.g. `"identifier"`, `"keyword_class"`).
/// @param type The token type to name.
/// @return Name string suitable for error messages.
std::string toString(StlTokenType type);

/// Ordered sequence of tokens produced by `StlScanner` and consumed by `StlParser`.
using StlTokenList = std::vector<StlToken>;

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_TOKEN_H
