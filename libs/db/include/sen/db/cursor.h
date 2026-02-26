// === cursor.h ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_CURSOR_H
#define SEN_DB_CURSOR_H

#include "sen/core/base/class_helpers.h"
#include "sen/core/base/timestamp.h"

#include <functional>
#include <variant>

namespace sen::db
{

class Input;

/// A cursor that can be used to iterate over the contents of a recording.
/// The cursor holds a payload which can have one of the values given as type arguments.
/// \ingroup db
template <typename End, typename... T>
class Cursor
{
  SEN_MOVE_ONLY(Cursor)

public:
  /// The content of the current entry.
  using Payload = std::variant<std::monostate, T..., End>;

  /// The current entry.
  struct Entry
  {
    TimeStamp time;   /// When.
    Payload payload;  /// What.
  };

public:
  ~Cursor() = default;

public:
  /// True if the cursor is at the end of the recording.
  [[nodiscard]] bool atEnd() const noexcept { return std::holds_alternative<End>(entry_.payload); }

  /// True if the cursor is at the start of the recording.
  [[nodiscard]] bool atBegining() const noexcept { return std::holds_alternative<std::monostate>(entry_.payload); }

  /// The current entry.
  [[nodiscard]] const Entry& get() const noexcept { return entry_; }

  /// The current entry.
  [[nodiscard]] const Entry& operator->() const noexcept { return entry_; }

  /// Advance the cursor one step.
  Cursor& operator++()
  {
    frontProvider_(entry_);
    return *this;
  }

private:
  friend class Input;
  using Provider = std::function<void(Entry&)>;

private:
  explicit Cursor(Provider frontProvider): frontProvider_(std::move(frontProvider)) {}

private:
  Entry entry_;
  Provider frontProvider_;
};

}  // namespace sen::db

#endif  // SEN_DB_CURSOR_H
