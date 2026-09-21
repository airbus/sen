// === method_printer.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_METHOD_PRINTER_H
#define SEN_COMPONENTS_EXPLORER_SRC_METHOD_PRINTER_H

// explorer
#include "editable_printer_maker.h"

// sen
#include "sen/core/meta/method.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/detail/work_queue.h"
#include "sen/core/obj/object.h"

// std
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

struct MethodResultState
{
  sen::Var lastResult;
  bool hasResult = false;
  std::string lastError;
};

class MethodPrinter
{

public:
  MethodPrinter(std::shared_ptr<const sen::Method> method,
                std::shared_ptr<sen::Object> owner,
                sen::impl::WorkQueue* queue);

public:
  void draw();

private:
  [[nodiscard]] std::string getMethodName() const;
  void methodLabel() const;
  void methodArguments();
  void executeButton() const;
  [[nodiscard]] bool areInputsValid() const;

private:
  std::string methodName_;
  std::shared_ptr<const sen::Method> method_ = nullptr;
  std::shared_ptr<sen::Object> owner_ = nullptr;
  sen::impl::WorkQueue* queue_ = nullptr;
  std::vector<std::tuple<sen::Arg, sen::Var, EditablePrinterFunc, bool>> argDrawers_;
  EditablePrinterMaker::PropertiesStateMap propertiesStateMap_;

  std::shared_ptr<MethodResultState> resultState_;
  EditablePrinterFunc resultPrinter_;

private:
  enum ButtonState
  {
    invalid,
    valid,
  };

  const std::map<const ButtonState, const ImColor> buttonStateLUT {{invalid, {255, 0, 0, 150}},
                                                                   {valid, {0, 255, 50, 150}}};
};

#endif  // SEN_COMPONENTS_EXPLORER_SRC_METHOD_PRINTER_H
