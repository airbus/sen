// === vm.cpp ==========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "sen/core/lang/vm.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/numbers.h"
#include "sen/core/base/result.h"
#include "sen/core/base/span.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/lang/stl_expression.h"
#include "sen/core/lang/stl_parser.h"
#include "sen/core/lang/stl_scanner.h"
#include "sen/core/lang/stl_statement.h"
#include "sen/core/meta/var.h"

// std
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <functional>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#ifdef WIN32
#  pragma warning(push)
#  pragma warning(disable: 4018)
#  pragma warning(disable: 4146)
#endif

// uncomment this to enable execution tracing
// #define TRACE_VM_EXECUTION

namespace sen::lang
{

namespace
{

// -------------------------------------------------------------------------------------------------------------
// Compiler
// -------------------------------------------------------------------------------------------------------------

class Compiler
{
  SEN_NOCOPY_NOMOVE(Compiler)

public:
  explicit Compiler(Chunk& chunk): chunk_(chunk) {}

  ~Compiler() = default;

public:
  void compile(const QueryStatement& statement)
  {
    if (statement.condition)
    {
      std::visit(*this, statement.condition->value);
      endCompilation();
    }
  }

public:
  void operator()(std::monostate expr) const { std::ignore = expr; }

  void operator()(const StlLiteralExpr& expr)
  {
    std::visit(sen::Overloaded {[this](uint8_t val) { emitConstant(val); },
                                [this](int16_t val) { emitConstant(val); },
                                [this](uint16_t val) { emitConstant(val); },
                                [this](int32_t val) { emitConstant(val); },
                                [this](uint32_t val) { emitConstant(val); },
                                [this](int64_t val) { emitConstant(val); },
                                [this](uint64_t val) { emitConstant(val); },
                                [this](float32_t val) { emitConstant(val); },
                                [this](float64_t val) { emitConstant(val); },
                                [this](bool val) { emitConstant(val); },
                                [this](const std::string& val) { emitConstant(val); },
                                [this, &expr](const std::monostate& /*val*/) { emitConstant(expr.value.lexeme()); },
                                [](const sen::Duration& /*val*/) { sen::throwRuntimeError("invalid literal value"); },
                                [](const sen::TimeStamp& /*val*/) { sen::throwRuntimeError("invalid literal value"); },
                                [](const sen::VarList& /*val*/) { sen::throwRuntimeError("invalid literal value"); },
                                [](const sen::VarMap& /*val*/) { sen::throwRuntimeError("invalid literal value"); },
                                [](const sen::KeyedVar& /*val*/) { sen::throwRuntimeError("invalid literal value"); }},
               static_cast<const Var::ValueType&>(expr.value.value()));
  }

  void operator()(const StlUnaryExpr& expr)
  {
    // compute the result
    std::visit(*this, expr.expr->value);

    // then negate it
    if (expr.op == StlUnaryOperator::logicalNot || expr.op == StlUnaryOperator::negate)
    {
      emitByte(OpCode::opNegate);
    }
  }

  void operator()(const StlBinaryExpr& expr)
  {
    std::visit(*this, expr.lhs->value);
    std::visit(*this, expr.rhs->value);

    switch (expr.op)
    {
      case StlBinaryOperator::eq:
        emitByte(OpCode::opEqual);
        break;

      case StlBinaryOperator::ne:
        emitByte(OpCode::opNotEqual);
        break;

      case StlBinaryOperator::gt:
        emitByte(OpCode::opGreaterThan);
        break;

      case StlBinaryOperator::ge:
        emitByte(OpCode::opGreaterOrEqualThan);
        break;

      case StlBinaryOperator::lt:
        emitByte(OpCode::opLowerThan);
        break;

      case StlBinaryOperator::le:
        emitByte(OpCode::opLowerOrEqualThan);
        break;

      case StlBinaryOperator::add:
        emitByte(OpCode::opAdd);
        break;

      case StlBinaryOperator::subtract:
        emitByte(OpCode::opSub);
        break;

      case StlBinaryOperator::multiply:
        emitByte(OpCode::opMul);
        break;

      case StlBinaryOperator::divide:
        emitByte(OpCode::opDiv);
        break;

      default:
        sen::throwRuntimeError("invalid binary operator");
    }
  }

  void operator()(const StlGroupingExpr& expr) { std::visit(*this, expr.expr->value); }

  void operator()(const StlLogicalExpr& expr)
  {
    switch (expr.op)
    {
      case StlLogicalOperator::logicalAnd:
      {
        std::visit(*this, expr.lhs->value);  // if this is false, do not continue
        auto offset = emitJump(OpCode::opJumpIfFalse);
        std::visit(*this, expr.rhs->value);
        emitByte(OpCode::opAnd);
        patchJump(offset);
      }
      break;

      case StlLogicalOperator::logicalOr:
      {
        std::visit(*this, expr.lhs->value);  // if this is true, do not continue
        auto offset = emitJump(OpCode::opJumpIfTrue);
        std::visit(*this, expr.rhs->value);
        emitByte(OpCode::opOr);
        patchJump(offset);
      }
      break;

      default:
        sen::throwRuntimeError("invalid logical operator");
    }
  }

  void operator()(const StlVariableExpr& expr)
  {
    auto index = chunk_.getOrRegisterVariable(expr.name);
    emitBytes(OpCode::opFetchVariable, index);
  }

  void operator()(const StlBetweenExpr& expr)
  {
    std::visit(*this, expr.lhs->value);
    std::visit(*this, expr.min->value);
    std::visit(*this, expr.max->value);
    emitByte(OpCode::opBetween);

    if (expr.notBetween)
    {
      emitByte(OpCode::opNegate);
    }
  }

  void operator()(const StlInExpr& expr)
  {
    std::vector<int> offsets;
    offsets.reserve(expr.set.size());

    for (const auto& elem: expr.set)
    {
      std::visit(*this, expr.lhs->value);
      std::visit(*this, elem->value);

      if (expr.notIn)
      {
        emitByte(OpCode::opNotEqual);
        offsets.push_back(emitJump(OpCode::opJumpIfFalse));
      }
      else
      {
        emitByte(OpCode::opEqual);
        offsets.push_back(emitJump(OpCode::opJumpIfTrue));
      }
    }

    for (auto offset: offsets)
    {
      patchJump(offset);
    }
  }

private:
  void emitByte(uint8_t byte) { chunk_.addCode(byte); }

  void emitBytes(uint8_t byte1, uint8_t byte2)
  {
    emitByte(byte1);
    emitByte(byte2);
  }

  void endCompilation() { emitReturn(); }

  void emitReturn() { emitByte(OpCode::opReturn); }

  int emitJump(uint8_t instruction)
  {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return chunk_.count() - 2;
  }

  void patchJump(int offset)
  {
    // -2 to adjust for the bytecode for the jump offset itself
    unsigned int jump = chunk_.count() - offset - 2;
    if (jump > std::numeric_limits<uint16_t>::max())
    {
      sen::throwRuntimeError("Too much code to jump over");
    }

    chunk_.patch(offset, (jump >> 8U) & 0xffU);
    chunk_.patch(offset + 1, jump & 0xffU);
  }

  void emitConstant(const Value& value) { emitBytes(OpCode::opConstant, chunk_.addConstant(value)); }

private:
  Chunk& chunk_;
};

template <typename C>
bool compare(const Value& a, const Value& b, C op)  // NOLINT
{
  if (holds<VariantAccessError>(a) || holds<VariantAccessError>(b))
  {
    return false;
  }

  if (holds<std::string>(a))
  {
    if (holds<std::string>(b))
    {
      return op(extract<std::string>(a), extract<std::string>(b));
    }

    throw std::logic_error("invalid value for comparison");
  }

  if (holds<bool>(a))
  {
    if (holds<bool>(b))
    {
      return op(extract<bool>(a), extract<bool>(b));
    }

    throw std::logic_error("invalid value for comparison");
  }

  // NOLINTNEXTLINE
#define CAST_RHS(rhsType)                                                                                              \
  if (holds<rhsType>(b))                                                                                               \
  {                                                                                                                    \
    rhsType rhs = extract<rhsType>(b);                                                                                 \
    return op(lhs, static_cast<rhsType>(rhs));                                                                         \
  }

  // NOLINTNEXTLINE
#define OP_TYPE(lhsType)                                                                                               \
  if (holds<lhsType>(a))                                                                                               \
  {                                                                                                                    \
    lhsType lhs = extract<lhsType>(a); /* NOLINT(modernize-use-auto) */                                                \
                                                                                                                       \
    if (holds<lhsType>(b))                                                                                             \
    {                                                                                                                  \
      lhsType rhs = extract<lhsType>(b); /* NOLINT(modernize-use-auto) */                                              \
      return op(lhs, rhs);                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    CAST_RHS(float32_t)                                                                                                \
    CAST_RHS(float64_t)                                                                                                \
    CAST_RHS(int32_t)                                                                                                  \
    CAST_RHS(uint32_t)                                                                                                 \
    CAST_RHS(int64_t)                                                                                                  \
    CAST_RHS(uint64_t)                                                                                                 \
    CAST_RHS(uint8_t)                                                                                                  \
    CAST_RHS(int16_t)                                                                                                  \
    CAST_RHS(uint16_t)                                                                                                 \
                                                                                                                       \
    throw std::logic_error("invalid value for comparison - " SEN_STRINGIFY(lhsType));                                  \
  }

  OP_TYPE(float32_t)
  OP_TYPE(float64_t)
  OP_TYPE(int32_t)
  OP_TYPE(uint32_t)
  OP_TYPE(int64_t)
  OP_TYPE(uint64_t)
  OP_TYPE(uint8_t)
  OP_TYPE(int16_t)
  OP_TYPE(uint16_t)

#undef CAST_RHS
#undef OP_TYPE

  throw std::logic_error("invalid value for comparison");
}

}  // namespace

// -------------------------------------------------------------------------------------------------------------
// Chunk
// -------------------------------------------------------------------------------------------------------------

const uint8_t* Chunk::code() const noexcept { return code_.data(); }

void Chunk::addCode(uint8_t byte) { code_.push_back(byte); }

void Chunk::patch(std::size_t offset, uint8_t byte) { code_.at(offset) = byte; }

const Value& Chunk::getConstant(uint8_t offset) const { return constants_[offset]; }

int Chunk::count() const noexcept { return static_cast<int>(code_.size()); }

sen::Span<const std::string> Chunk::getVariables() const
{
  return sen::makeConstSpan(variablesDef_.begin(), variablesDef_.end());
}

bool Chunk::isValid() const noexcept { return !code_.empty(); }

void Chunk::disassemble(const std::string& name) const
{
  printf("== %s ==\n", name.c_str());  // NOLINT

  for (std::size_t offset = 0U; offset < code_.size();)
  {
    offset = disassembleInstruction(offset);
  }
}

std::size_t Chunk::disassembleInstruction(std::size_t offset) const
{
  // offset
  printf("%04zu  ", offset);  // NOLINT

  const auto instruction = code_[offset];
  switch (instruction)  // NOLINT
  {
    case OpCode::opReturn:
      return printSimpleInstruction("return", offset);
    case OpCode::opConstant:
      return printConstantInstruction("constant", offset);
    case OpCode::opNegate:
      return printSimpleInstruction("negate", offset);
    case OpCode::opAdd:
      return printSimpleInstruction("add", offset);
    case OpCode::opSub:
      return printSimpleInstruction("subtract", offset);
    case OpCode::opMul:
      return printSimpleInstruction("multiply", offset);
    case OpCode::opDiv:
      return printSimpleInstruction("divide", offset);
    case OpCode::opEqual:
      return printSimpleInstruction("equals", offset);
    case OpCode::opNotEqual:
      return printSimpleInstruction("not_equals", offset);
    case OpCode::opGreaterThan:
      return printSimpleInstruction("greater_than", offset);
    case OpCode::opGreaterOrEqualThan:
      return printSimpleInstruction("greater_or_equal_than", offset);
    case OpCode::opLowerThan:
      return printSimpleInstruction("less_than", offset);
    case OpCode::opLowerOrEqualThan:
      return printSimpleInstruction("less_or_equal_than", offset);
    case OpCode::opAnd:
      return printSimpleInstruction("and", offset);
    case OpCode::opOr:
      return printSimpleInstruction("or", offset);
    case OpCode::opJumpIfFalse:
      return printJumpInstruction("jump_if_false", offset);
    case OpCode::opJumpIfTrue:
      return printJumpInstruction("jump_if_true", offset);
    case OpCode::opFetchVariable:
      return printVariableInstruction(offset);
    case OpCode::opBetween:
      return printSimpleInstruction("between", offset);

    default:
      printf("Unknown opcode %d\n", instruction);  // NOLINT
      return offset + 1;
  }
}

uint8_t Chunk::addConstant(const Value& value)
{
  if (constants_.full())
  {
    sen::throwRuntimeError("too many constants");
  }

  auto offset = constants_.size();
  constants_.push_back(value);
  return static_cast<uint8_t>(offset);
}

uint8_t Chunk::getOrRegisterVariable(const std::string& name)
{
  if (variablesDef_.full())
  {
    sen::throwRuntimeError("too many variables");
  }

  // return the index if it's already registered
  for (uint8_t i = 0; i < static_cast<uint8_t>(variablesDef_.size()); ++i)
  {
    if (variablesDef_[i] == name)
    {
      return i;
    }
  }

  variablesDef_.push_back(name);
  return static_cast<uint8_t>(variablesDef_.size() - 1U);
}

std::size_t Chunk::printSimpleInstruction(std::string_view name, std::size_t offset) const
{
  printf("%s\n", name.data());  // NOLINT
  return offset + 1U;
}

std::size_t Chunk::printConstantInstruction(std::string_view name, std::size_t offset) const
{
  auto constantIndex = code_[offset + 1];
  printf("%-16s %04u '", name.data(), constantIndex);  // NOLINT
  printValue(constants_[constantIndex]);
  printf("'\n");       // NOLINT
  return offset + 2U;  // one instruction + one operand
}

std::size_t Chunk::printJumpInstruction(std::string_view name, std::size_t offset) const
{
  auto jump = (uint16_t)(code_[offset + 1] << 8);  // NOLINT
  jump |= code_[offset + 2];
  printf("%-16s %04zu -> %04zu\n", name.data(), offset, offset + 3 + jump);  // NOLINT
  return offset + 3;
}

std::size_t Chunk::printVariableInstruction(std::size_t offset) const
{
  auto variableIndex = code_[offset + 1];
  printf("fetch_variable   %04u '%s'\n", variableIndex, variablesDef_[variableIndex].c_str());  // NOLINT
  return offset + 2U;
}

void Chunk::printValue(const Value& value)
{
  if (holds<bool>(value))
  {
    printf("<bool> %s", extract<bool>(value) ? "true" : "false");  // NOLINT
    return;
  }

  if (holds<std::string>(value))
  {
    printf("<str> %s", extract<std::string>(value).c_str());  // NOLINT
    return;
  }

  // NOLINTNEXTLINE
#define PRINT_NUMBER(type)                                                                                             \
  if (holds<type>(value))                                                                                              \
  {                                                                                                                    \
    printf("<%s> %s", #type, std::to_string(extract<type>(value)).c_str());                                            \
    return;                                                                                                            \
  }

  PRINT_NUMBER(i32)  // NOLINT
  PRINT_NUMBER(u32)  // NOLINT
  PRINT_NUMBER(i64)  // NOLINT
  PRINT_NUMBER(u64)  // NOLINT
  PRINT_NUMBER(f32)  // NOLINT
  PRINT_NUMBER(f64)  // NOLINT
  PRINT_NUMBER(u8)   // NOLINT
  PRINT_NUMBER(i16)  // NOLINT
  PRINT_NUMBER(u16)  // NOLINT

#undef PRINT_NUMBER

  sen::throwRuntimeError("invalid value");
}

// -------------------------------------------------------------------------------------------------------------
// VM
// -------------------------------------------------------------------------------------------------------------

QueryStatement VM::parse(const std::string& query) const
{
  StlScanner scanner(query);
  const auto& tokens = scanner.scanTokens();
  StlParser parser(tokens);
  return parser.parseQuery();
}

Result<Chunk, VM::CompileError> VM::compile(const QueryStatement& statement) const
{
  Chunk chunk;
  Compiler compiler(chunk);

  try
  {
    compiler.compile(statement);
  }
  catch (const std::exception& err)
  {
    return Err(CompileError {err.what()});
  }

  return Ok(std::move(chunk));
}

Result<Value, VM::RuntimeError> VM::interpret(const Chunk& chunk, Environment environment)
{
  currentChunk_ = &chunk;
  environment_ = environment;
  ip_ = chunk.code();

  for (;;)
  {
#ifdef TRACE_VM_EXECUTION
    auto stackCopy = stack_;
    while (!stackCopy.empty())
    {
      printf("[ ");  // NOLINT
      Chunk::printValue(stackCopy.top());
      printf(" ] ");  // NOLINT
      stackCopy.pop();
    }
    printf("\n");  // NOLINT

    std::ignore = currentChunk_->disassembleInstruction(static_cast<std::size_t>(ip_ - chunk.code()));
#endif

    const auto instruction = readByte();

    switch (instruction)
    {
      case OpCode::opReturn:
        return Ok(pop());

      case OpCode::opConstant:
        push(readConstant());
        break;

      case OpCode::opNegate:
        negate();
        break;

      case OpCode::opAdd:
        add();
        break;

      case OpCode::opEqual:
        equal();
        break;

      case OpCode::opNotEqual:
        notEqual();
        break;

      case OpCode::opGreaterThan:
        greaterThan();
        break;

      case OpCode::opGreaterOrEqualThan:
        greaterOrEqualThan();
        break;

      case OpCode::opLowerThan:
        lowerThan();
        break;

      case OpCode::opLowerOrEqualThan:
        lowerOrEqualThan();
        break;

      case OpCode::opAnd:
        doAnd();
        break;

      case OpCode::opOr:
        doOr();
        break;

      case OpCode::opSub:
        subtract();
        break;

      case OpCode::opMul:
        multiply();
        break;

      case OpCode::opDiv:
        divide();
        break;

      case OpCode::opJumpIfFalse:
        jumpIfFalse(readShort());
        break;

      case OpCode::opJumpIfTrue:
        jumpIfTrue(readShort());
        break;

      case OpCode::opFetchVariable:
        fetchVariable(readByte());
        break;

      case OpCode::opBetween:
        between();
        break;

      default:
        return Err(RuntimeError {"invalid instruction"});
    }
  }
}

void VM::negate()
{
  auto value = pop();

  if (holds<bool>(value))
  {
    push(!extract<bool>(value));
    return;
  }

  // NOLINTNEXTLINE
#define NEGATE_NUMBER(type)                                                                                            \
  if (holds<type>(value))                                                                                              \
  {                                                                                                                    \
    push(-extract<type>(value));                                                                                       \
    return;                                                                                                            \
  }

  NEGATE_NUMBER(uint32_t)
  NEGATE_NUMBER(int32_t)
  NEGATE_NUMBER(float32_t)
  NEGATE_NUMBER(float64_t)
  NEGATE_NUMBER(uint64_t)
  NEGATE_NUMBER(int64_t)
  NEGATE_NUMBER(uint16_t)
  NEGATE_NUMBER(int16_t)
  NEGATE_NUMBER(uint8_t)
#undef NEGATE_NUMBER

  throw std::logic_error("invalid type for negation");
}

void VM::add()
{
  const auto b = pop();
  const auto a = pop();
  mathOperation(a, b, std::plus<>());
}

void VM::subtract()
{
  const auto b = pop();
  const auto a = pop();
  mathOperation(a, b, std::minus<>());
}

void VM::multiply()
{
  const auto b = pop();
  const auto a = pop();
  mathOperation(a, b, std::multiplies<>());
}

void VM::divide()
{
  const auto b = pop();
  const auto a = pop();
  mathOperation(a, b, std::divides<>());
}

void VM::equal()
{
  const auto b = pop();
  const auto a = pop();
  comparisonOperation(a, b, std::equal_to<>());
}

void VM::notEqual()
{
  const auto b = pop();
  const auto a = pop();
  comparisonOperation(a, b, std::not_equal_to<>());
}

void VM::lowerThan()
{
  const auto b = pop();
  const auto a = pop();
  comparisonOperation(a, b, std::less<>());
}

void VM::lowerOrEqualThan()
{
  const auto b = pop();
  const auto a = pop();
  comparisonOperation(a, b, std::less_equal<>());
}

void VM::greaterThan()
{
  const auto b = pop();
  const auto a = pop();
  comparisonOperation(a, b, std::greater<>());
}

void VM::greaterOrEqualThan()
{
  const auto b = pop();
  const auto a = pop();
  comparisonOperation(a, b, std::greater_equal<>());
}

void VM::doAnd()
{
  const auto b = pop();
  const auto a = pop();
  push(extract<bool>(a) && extract<bool>(b));
}

void VM::doOr()
{
  const auto b = pop();
  const auto a = pop();
  push(extract<bool>(a) || extract<bool>(b));
}

void VM::jumpIfFalse(uint16_t offset)
{
  if (!extract<bool>(stack_.top()))
  {
    ip_ += offset;  // NOLINT
  }
}

void VM::jumpIfTrue(uint16_t offset)
{
  if (extract<bool>(stack_.top()))
  {
    ip_ += offset;  // NOLINT
  }
}

void VM::fetchVariable(uint8_t index)
{
  try
  {
    push(environment_[index]());  // NOLINT
  }
  catch (const std::bad_variant_access& err)
  {
    std::ignore = err;
    push(VariantAccessError {});
  }
}

void VM::between()
{
  const auto max = pop();
  const auto min = pop();
  const auto val = pop();

  if (compare(min, max, std::greater {}))
  {
    sen::throwRuntimeError("expression: 'BETWEEN 'min' AND 'max'' expects 'min' less or equal than 'max'");
  }

  push(compare(val, min, std::greater_equal {}) && compare(val, max, std::less_equal {}));
}

void VM::push(Value value) { stack_.push(std::move(value)); }

Value VM::pop()
{
  auto top = stack_.top();
  stack_.pop();
  return top;
}

uint8_t VM::readByte()
{
  return (*ip_++);  // NOLINT
}

uint16_t VM::readShort()
{
  ip_ += 2;                                                // NOLINT
  return static_cast<uint16_t>((ip_[-2] << 8) | ip_[-1]);  // NOLINT
}

Value VM::readConstant() { return currentChunk_->getConstant(readByte()); }

template <typename C>
void VM::mathOperation(const Value& a, const Value& b, C op)  // NOLINT
{
  if (holds<VariantAccessError>(a) || holds<VariantAccessError>(b))
  {
    push(VariantAccessError {});
    return;
  }

  // NOLINTNEXTLINE
#define CAST_RHS(rhsType)                                                                                              \
  if (holds<rhsType>(b))                                                                                               \
  {                                                                                                                    \
    auto rhs = extract<rhsType>(b);                                                                                    \
    push(op(lhs, static_cast<rhsType>(rhs)));                                                                          \
    return;                                                                                                            \
  }

  // NOLINTNEXTLINE
#define OP_TYPE(lhsType)                                                                                               \
  if (holds<lhsType>(a))                                                                                               \
  {                                                                                                                    \
    const auto lhs = extract<lhsType>(a);                                                                              \
                                                                                                                       \
    if (holds<lhsType>(b))                                                                                             \
    {                                                                                                                  \
      push(op(lhs, extract<lhsType>(b)));                                                                              \
      return;                                                                                                          \
    }                                                                                                                  \
                                                                                                                       \
    CAST_RHS(float32_t)                                                                                                \
    CAST_RHS(float64_t)                                                                                                \
    CAST_RHS(int32_t)                                                                                                  \
    CAST_RHS(uint32_t)                                                                                                 \
    CAST_RHS(int64_t)                                                                                                  \
    CAST_RHS(uint64_t)                                                                                                 \
    CAST_RHS(uint8_t)                                                                                                  \
    CAST_RHS(int16_t)                                                                                                  \
    CAST_RHS(uint16_t)                                                                                                 \
                                                                                                                       \
    throw std::logic_error("invalid value for math");                                                                  \
  }

  OP_TYPE(float32_t)
  OP_TYPE(float64_t)
  OP_TYPE(int32_t)
  OP_TYPE(uint32_t)
  OP_TYPE(int64_t)
  OP_TYPE(uint64_t)
  OP_TYPE(uint8_t)
  OP_TYPE(int16_t)
  OP_TYPE(uint16_t)

#undef CAST_RHS
#undef OP_TYPE

  throw std::logic_error("invalid value for math");
}

template <typename C>
void VM::comparisonOperation(const Value& a, const Value& b, C op)  // NOLINT
{
  push(compare(a, b, op));
}

}  // namespace sen::lang

#ifdef WIN32
#  pragma warning(pop)
#endif
