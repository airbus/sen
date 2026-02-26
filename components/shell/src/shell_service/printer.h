// === printer.h =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_PRINTER_H
#define SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_PRINTER_H

// generated code
#include "stl/shell.stl.h"

// sen
#include "sen/core/meta/class_type.h"
#include "sen/core/obj/object.h"

// component
#include "terminal.h"

// std
#include <list>

namespace sen::components::shell
{

class Printer final
{
public:
  SEN_MOVE_ONLY(Printer)

public:
  Printer(Terminal* terminal, BufferStyle bufferStyle, TimeStyle timeStyle);
  ~Printer() = default;

public:
  static void printWelcome(Terminal* terminal);
  void printError(const char* fmt, ...) const;
  void printValue(const Var& value, size_t level, const Type* type) const;
  void printProperties(Object* object) const;
  void printPropertyValue(const Property* prop, const Var& value) const;
  void printMethodCallResult(const MethodResult<Var>& var, const Method* method, std::string_view command) const;
  void printDescription(const ClassType& owner, const Method* method) const;
  void printDescription(const Type& type) const;
  void printDescription(const Object& instance) const;
  void printInstances(const std::list<Object*>& instances) const;
  void printEnumError(const std::string& argName, const EnumType* type) const;

public:
  void printTitle(std::string_view title) const;

  static void printTable(const std::vector<std::string>& header,
                         const std::vector<std::vector<std::string>>& table,
                         unsigned indentation,
                         bool printHeader,
                         Terminal* terminal,
                         const Style& style = {});

  static std::string formatTextForWidth(const std::string& text, size_t width, size_t indent);

private:
  Terminal* terminal_;
  BufferStyle bufferStyle_;
  TimeStyle timeStyle_;
};

}  // namespace sen::components::shell

#endif  // SEN_COMPONENTS_SHELL_SRC_SHELL_SERVICE_PRINTER_H
