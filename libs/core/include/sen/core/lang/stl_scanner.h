// === stl_scanner.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_STL_SCANNER_H
#define SEN_CORE_LANG_STL_SCANNER_H

#include "sen/core/base/class_helpers.h"
#include "sen/core/lang/code_location.h"
#include "sen/core/lang/stl_token.h"

// std
#include <string>

namespace sen
{
struct Var;
}

namespace sen::lang
{

/// \addtogroup lang
/// @{

/// Lexer that converts a raw STL source string into a flat list of tokens.
/// The first stage of the STL pipeline: source text â†’ `StlTokenList` â†’ `StlParser` â†’ AST.
class StlScanner
{
  SEN_NOCOPY_NOMOVE(StlScanner)

public:
  /// Constructs the scanner over the given source text.
  /// @param program STL source string to scan; the reference must remain valid
  ///                until `scanTokens()` returns.
  explicit StlScanner(const std::string& program);
  ~StlScanner() = default;

public:
  /// Scans the full source string and returns the resulting token list.
  /// The returned reference is valid for the lifetime of this scanner object.
  /// @return Reference to the internal token list, always terminated by an `endOfFile` token.
  /// @throws std::runtime_error if an unexpected character or malformed literal is encountered.
  [[nodiscard]] const StlTokenList& scanTokens();

private:
  enum class Sign
  {
    positive,
    negative
  };

private:
  void scanToken();
  void addToken(StlTokenType type, const Var& value);
  void error(const std::string& message) const;
  void string(char endingChar);
  void comment();
  void ruler();
  void number(Sign sign);
  void identifier();

private:  // utility
  [[nodiscard]] bool isAtEnd() const;
  [[nodiscard]] char advance();
  [[nodiscard]] bool match(char expected);
  [[nodiscard]] char peek() const;
  [[nodiscard]] char peekNext() const;
  [[nodiscard]] StlToken identifierToken() const;
  [[nodiscard]] static std::string formatNumber(const std::string& lexeme);

private:
  const std::string& program_;
  StlTokenList tokens_;
  std::size_t start_ = 0U;
  CodeLocation current_;
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_STL_SCANNER_H
