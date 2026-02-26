// === interpreter_obj.cpp =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "interpreter_obj.h"

// component
#include "component_api.h"

// sen
#include "sen/core/base/assert.h"

// pybind11
#include <pybind11/detail/common.h>
#include <pybind11/eval.h>
#include <pybind11/gil.h>
#include <pybind11/pytypes.h>

// std
#include <functional>
#include <string>

namespace sen::components::py
{

std::string InterpreterObj::evalImpl(const std::string& expr)
{
  pybind11::object result;
  run([&result, &expr]() { result = pybind11::eval(expr); });
  return result ? pybind11::str(result) : "";
}

void InterpreterObj::execImpl(const std::string& code)
{
  run([&code]() { pybind11::exec(code); });
}

void InterpreterObj::execFileImpl(const std::string& file)
{
  run([&file]() { pybind11::eval_file(file); });
}

void InterpreterObj::run(std::function<void()>&& function) const
{
  pybind11::gil_scoped_acquire acquire;
  try
  {
    function();
  }
  catch (pybind11::error_already_set& err)
  {
    getLogger()->error(err.what());
    err.discard_as_unraisable("py::interpreter");
  }
  catch (const pybind11::attribute_error& err)
  {
    getLogger()->error(err.what());
    throwRuntimeError(err.what());
  }
  catch (...)
  {
    getLogger()->error("interpreter: unknown error");
    throwRuntimeError("interpreter: unknown error");
  }
}

}  // namespace sen::components::py
