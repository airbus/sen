// === vm.h ============================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_CORE_LANG_VM_H
#define SEN_CORE_LANG_VM_H

// sen
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/static_vector.h"
#include "sen/core/lang/stl_statement.h"

// std
#include <cstdint>
#include <functional>
#include <stack>
#include <string>
#include <variant>
#include <vector>

namespace sen::lang
{

/// \addtogroup lang
/// @{

constexpr std::size_t stackMax = 256U;

/// Instructions that can be executed by the virtual machine
enum OpCode : uint8_t
{
  opReturn = 0,               ///< Finishes execution.
  opConstant = 1,             ///< Push a constant to the stack. Requires an index as the argument.
  opNegate = 2,               ///< Negates the value at the top of the stack.
  opAdd = 3,                  ///< Adds the two top-most values on the stack.
  opSub = 4,                  ///< Subtracts the two top-most values on the stack.
  opMul = 5,                  ///< Multiplies the two top-most values on the stack.
  opDiv = 6,                  ///< Divides the two top-most values on the stack.
  opEqual = 7,                ///< Compares the two top-most values for equality
  opNotEqual = 8,             ///< Compares the two top-most values for inequality
  opGreaterThan = 9,          ///< Compares the two top-most values for >
  opGreaterOrEqualThan = 10,  ///< Compares the two top-most values for >=
  opLowerThan = 11,           ///< Compares the two top-most values for <
  opLowerOrEqualThan = 12,    ///< Compares the two top-most values for <=
  opAnd = 13,                 ///< Logical and of the two top-most values
  opOr = 14,                  ///< Logical or of the two top-most values
  opJumpIfTrue = 15,          ///< Jumps to an offset if the top-most value is true
  opJumpIfFalse = 16,         ///< Jumps to an offset if the top-most value is false
  opFetchVariable = 17,       ///< Fetches a variable in a given index and pushes the value
  opBetween = 18              ///< Checks that the top and second values are <= and >= to the third value
};

/// Represents a variant that could not be accessed.
struct VariantAccessError
{
};

/// A value that can be in the stack.
using Value = std::variant<float32_t,
                           float64_t,
                           int32_t,
                           uint32_t,
                           int64_t,
                           uint64_t,
                           bool,
                           std::string,
                           uint8_t,
                           int16_t,
                           uint16_t,
                           VariantAccessError>;

/// Gets a value from some source.
using ValueGetter = std::function<Value()>;

/// The environment is an indexed list of value getter functions.
using Environment = Span<ValueGetter>;

/// Get T out of the value variant.
template <typename T>
[[nodiscard]] inline T extract(const Value& value)
{
  return std::get<T>(value);
}

/// True if the variant holds T.
template <typename T>
[[nodiscard]] inline bool holds(const Value& value) noexcept
{
  return std::holds_alternative<T>(value);
}

/// A chunk of byte code.
/// It holds code and constants needed by the code.
/// Constants are accessed by index.
class Chunk
{
  SEN_MOVE_ONLY(Chunk)

public:  // special members
  Chunk() = default;
  ~Chunk() = default;

public:
  /// Prints a human-readable view of the chunk
  void disassemble(const std::string& name) const;

  /// Stores a constant and returns the index that can be used to refer to it
  [[nodiscard]] uint8_t addConstant(const Value& value);

  /// The constant stored at a given slot
  [[nodiscard]] const Value& getConstant(uint8_t offset) const;

  /// Appends a byte to the code. The line is just for debugging.
  void addCode(uint8_t byte);

  /// The variables used by this chunk.
  [[nodiscard]] Span<const std::string> getVariables() const;

  /// Registers a variable and obtains the index.
  [[nodiscard]] uint8_t getOrRegisterVariable(const std::string& name);

  /// Returns true if it contains code
  [[nodiscard]] bool isValid() const noexcept;

  /// The stored code.
  [[nodiscard]] const uint8_t* code() const noexcept;

  /// The size of the code
  [[nodiscard]] int count() const noexcept;

  /// Patches the a code byte at some offset
  void patch(std::size_t offset, uint8_t byte);

  /// Prints a human-readable view of the instruction at a given offset
  [[nodiscard]] std::size_t disassembleInstruction(std::size_t offset) const;

  /// Prints a value.
  static void printValue(const Value& value);

private:
  [[nodiscard]] std::size_t printSimpleInstruction(std::string_view name, std::size_t offset) const;
  [[nodiscard]] std::size_t printConstantInstruction(std::string_view name, std::size_t offset) const;
  [[nodiscard]] std::size_t printJumpInstruction(std::string_view name, std::size_t offset) const;
  [[nodiscard]] std::size_t printVariableInstruction(std::size_t offset) const;

private:
  std::vector<uint8_t> code_;
  StaticVector<Value, stackMax> constants_;
  StaticVector<std::string, stackMax> variablesDef_;
};

/// Virtual machine for executing Sen byte code.
class VM
{
  SEN_MOVE_ONLY(VM)

public:
  VM() = default;
  ~VM() = default;

public:
  /// Compilation failure report.
  struct CompileError
  {
    std::string what;
  };

  /// Runtime error report.
  struct RuntimeError
  {
    std::string what;
  };

public:
  /// Interpret a chunk of code.
  [[nodiscard]] Result<Value, RuntimeError> interpret(const Chunk& chunk, Environment environment = {});

  /// Parse a query string into a statement. Throws in case of error.
  [[nodiscard]] QueryStatement parse(const std::string& query) const;

  /// Compile source code into a chunk.
  [[nodiscard]] Result<Chunk, CompileError> compile(const QueryStatement& statement) const;

private:
  [[nodiscard]] SEN_ALWAYS_INLINE uint8_t readByte();
  [[nodiscard]] SEN_ALWAYS_INLINE uint16_t readShort();
  [[nodiscard]] SEN_ALWAYS_INLINE Value readConstant();
  [[nodiscard]] inline Value pop();
  inline void push(Value value);
  inline void negate();
  inline void add();
  inline void subtract();
  inline void multiply();
  inline void divide();
  inline void equal();
  inline void notEqual();
  inline void lowerThan();
  inline void lowerOrEqualThan();
  inline void greaterThan();
  inline void greaterOrEqualThan();
  inline void doAnd();
  inline void doOr();
  inline void jumpIfFalse(uint16_t offset);
  inline void jumpIfTrue(uint16_t offset);
  inline void fetchVariable(uint8_t index);
  inline void between();

private:
  template <typename C>
  inline void mathOperation(const Value& a, const Value& b, C op);

  template <typename C>
  inline void comparisonOperation(const Value& a, const Value& b, C op);

private:
  using Stack = std::stack<Value, StaticVector<Value, stackMax>>;

private:
  const Chunk* currentChunk_ = nullptr;
  Environment environment_;
  const uint8_t* ip_ = nullptr;
  Stack stack_;
};

/// @}

}  // namespace sen::lang

#endif  // SEN_CORE_LANG_VM_H
