// === interpreter_obj.h ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_PY_SRC_INTERPRETER_OBJ_H
#define SEN_COMPONENTS_PY_SRC_INTERPRETER_OBJ_H

// generated code
#include "stl/python_interpreter.stl.h"

namespace sen::components::py
{

class InterpreterObj: public PythonInterpreterBase
{
  SEN_NOCOPY_NOMOVE(InterpreterObj)

public:
  using PythonInterpreterBase::PythonInterpreterBase;
  ~InterpreterObj() override = default;

protected:
  [[nodiscard]] std::string evalImpl(const std::string& expr) override;

  void execImpl(const std::string& code) override;

  void execFileImpl(const std::string& file) override;

private:
  void run(std::function<void()>&& function) const;
};

}  // namespace sen::components::py

#endif  // SEN_COMPONENTS_PY_SRC_INTERPRETER_OBJ_H
