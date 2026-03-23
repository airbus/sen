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

/// Extracts the alternative `T` from a `Value` variant.
/// @tparam T One of the types in `Value` (e.g. `int32_t`, `float64_t`, `bool`).
/// @param value The variant to extract from.
/// @return The stored value of type `T`.
/// @throws std::bad_variant_access if `value` does not currently hold type `T`.
template <typename T>
[[nodiscard]] inline T extract(const Value& value)
{
  return std::get<T>(value);
}

/// Checks whether a `Value` variant currently holds the alternative `T`.
/// @tparam T One of the types in `Value` (e.g. `int32_t`, `float64_t`, `bool`).
/// @param value The variant to inspect.
/// @return `true` if `value` holds `T`, `false` otherwise.
template <typename T>
[[nodiscard]] inline bool holds(const Value& value) noexcept
{
  return std::holds_alternative<T>(value);
}

/// A compiled unit of Sen byte code.
/// Holds the raw instruction bytes and the constant pool referenced by `opConstant`.
/// Constants are addressed by an 8-bit index, so a chunk supports up to 256 constants.
class Chunk
{
  SEN_MOVE_ONLY(Chunk)

public:  // special members
  Chunk() = default;
  ~Chunk() = default;

public:
  /// Disassembles and prints the entire chunk to stdout for debugging.
  /// @param name Label displayed in the header of the disassembly output.
  void disassemble(const std::string& name) const;

  /// Appends a constant to the pool and returns its index.
  /// @param value The constant value to store.
  /// @return 8-bit index that can be used with `opConstant` to push this value.
  [[nodiscard]] uint8_t addConstant(const Value& value);

  /// Returns the constant at the given pool index.
  /// @param offset Index returned by a previous call to `addConstant()`.
  /// @return Reference to the stored constant; valid for the lifetime of this chunk.
  [[nodiscard]] const Value& getConstant(uint8_t offset) const;

  /// Appends a single instruction byte to the code buffer.
  /// @param byte Raw opcode or inline argument byte to append.
  void addCode(uint8_t byte);

  /// Returns the list of variable names referenced by this chunk.
  /// @return Non-owning span over the internal variable-name table; valid for the lifetime of this chunk.
  [[nodiscard]] Span<const std::string> getVariables() const;

  /// Looks up a variable by name, registering it if not yet known.
  /// @param name Variable name as it appears in the query expression.
  /// @return 8-bit index used by `opFetchVariable` to retrieve the variable's value at runtime.
  [[nodiscard]] uint8_t getOrRegisterVariable(const std::string& name);

  /// Returns `true` if the chunk contains at least one instruction byte.
  [[nodiscard]] bool isValid() const noexcept;

  /// Returns a pointer to the raw instruction bytes.
  /// @return Pointer to the beginning of the code buffer; valid for the lifetime of this chunk.
  [[nodiscard]] const uint8_t* code() const noexcept;

  /// Returns the number of instruction bytes in the code buffer.
  [[nodiscard]] int count() const noexcept;

  /// Overwrites a single byte in the code buffer (used for back-patching jump targets).
  /// @param offset Position in the code buffer to overwrite.
  /// @param byte   New byte value to write at that position.
  void patch(std::size_t offset, uint8_t byte);

  /// Disassembles and prints the instruction at the given offset.
  /// @param offset Byte offset into the code buffer.
  /// @return Offset of the next instruction (i.e. `offset + instruction size`).
  [[nodiscard]] std::size_t disassembleInstruction(std::size_t offset) const;

  /// Prints a human-readable representation of a `Value` to stdout.
  /// @param value The value to print.
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

/// Stack-based virtual machine for executing compiled Sen query byte code.
/// Typical usage: `parse()` a query string â†’ `compile()` the statement â†’ `interpret()` the chunk.
class VM
{
  SEN_MOVE_ONLY(VM)

public:
  VM() = default;
  ~VM() = default;

public:
  /// Describes a compilation failure returned by `compile()`.
  struct CompileError
  {
    std::string what;  ///< Human-readable description of the compilation error.
  };

  /// Describes a runtime failure returned by `interpret()`.
  struct RuntimeError
  {
    std::string what;  ///< Human-readable description of the runtime error.
  };

public:
  /// Executes a compiled chunk and returns the result value left on the stack.
  /// @param chunk       The compiled byte-code chunk to run.
  /// @param environment Variable bindings indexed by the variable table in `chunk`.
  /// @return `Ok(value)` on success, or `Err(RuntimeError)` if execution fails.
  [[nodiscard]] Result<Value, RuntimeError> interpret(const Chunk& chunk, Environment environment = {});

  /// Lexes and parses a Sen Query Language string into an AST statement.
  /// @param query  The query expression string to parse (e.g. `"SELECT * FROM bus WHERE x > 0"`).
  /// @return The parsed `QueryStatement` ready for compilation.
  /// @throws std::runtime_error if the query string is syntactically invalid.
  [[nodiscard]] QueryStatement parse(const std::string& query) const;

  /// Compiles a parsed query statement into an executable `Chunk`.
  /// @param statement  AST produced by `parse()`.
  /// @return `Ok(chunk)` on success, or `Err(CompileError)` if the statement cannot be compiled.
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
