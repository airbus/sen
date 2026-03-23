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

/// Forward-only iterator over the entries of a Sen recording.
///
/// Each entry carries a timestamp and a `Payload` variant holding one of the recorded
/// event types (`T...`), a sentinel `End` value when the recording is exhausted, or
/// `std::monostate` before the first call to `operator++`.
///
/// Typical usage:
/// @code
/// auto cursor = input.makeCursor();
/// for (++cursor; !cursor.atEnd(); ++cursor)
/// {
///   auto& entry = cursor.get();
///   if (auto* creation = std::get_if<Creation>(&entry.payload))
///     process(*creation);
/// }
/// @endcode
///
/// @tparam End  Sentinel type stored in the payload when the recording is exhausted.
/// @tparam T    Payload types produced by the recording (e.g. `Creation`, `Update`, `Event`).
/// \ingroup db
template <typename End, typename... T>
class Cursor
{
  SEN_MOVE_ONLY(Cursor)

public:
  /// Variant holding the current entry content.
  /// `std::monostate` â€” cursor not yet advanced (before first `++`).
  /// `T...`           â€” one of the recorded entry types.
  /// `End`            â€” recording exhausted (no more entries).
  using Payload = std::variant<std::monostate, T..., End>;

  /// A single timestamped entry from the recording.
  struct Entry
  {
    TimeStamp time;   ///< Timestamp recorded by the source component at emission time.
    Payload payload;  ///< The entry content; inspect with `std::get` or `std::get_if`.
  };

public:
  ~Cursor() = default;

public:
  /// @return True if the recording is exhausted and no more entries can be read.
  [[nodiscard]] bool atEnd() const noexcept { return std::holds_alternative<End>(entry_.payload); }

  /// @return True if the cursor has not yet been advanced (payload holds `std::monostate`).
  [[nodiscard]] bool atBegining() const noexcept { return std::holds_alternative<std::monostate>(entry_.payload); }

  /// @return Reference to the current entry; valid until the next call to `operator++`.
  [[nodiscard]] const Entry& get() const noexcept { return entry_; }

  /// @return Reference to the current entry (same as `get()`).
  [[nodiscard]] const Entry& operator->() const noexcept { return entry_; }

  /// Advances the cursor to the next entry in the recording.
  /// @return Reference to `*this` for chaining.
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
