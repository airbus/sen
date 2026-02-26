// === object_wrapper.h ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_PY_SRC_OBJECT_WRAPPER_H
#define SEN_COMPONENTS_PY_SRC_OBJECT_WRAPPER_H

// sen
#include "sen/core/meta/property.h"
#include "sen/core/obj/native_object.h"
#include "sen/core/obj/object.h"

// pybind11
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

namespace sen::components::py
{

// forward declarations
class RunApi;

/// Wraps a Sen object to offer it to the Python world
class ObjectWrapper
{
  SEN_COPY_MOVE(ObjectWrapper)

public:
  ~ObjectWrapper() = default;

public:
  [[nodiscard]] static ObjectWrapper makeGeneric(std::shared_ptr<Object> object, RunApi* api);

  [[nodiscard]] static ObjectWrapper makeNative(std::shared_ptr<NativeObject> object, RunApi* api);

public:
  [[nodiscard]] std::shared_ptr<Object> get() const noexcept;
  [[nodiscard]] std::shared_ptr<NativeObject> getNative() const noexcept;

public:
  static void definePythonApi(pybind11::module_& args);

private:
  [[nodiscard]] pybind11::object callMethod(const Method* method, const pybind11::args& args);
  void setAttribute(const std::string& name, pybind11::object value);
  [[nodiscard]] pybind11::object getAttribute(const std::string& name);
  [[nodiscard]] const Property* searchPropertyChange(const std::string& name) const;
  [[nodiscard]] const Event* searchEvent(const std::string& name) const;
  void installChangeCallback(const Property* prop, const pybind11::args& args);
  void installEventCallback(const Event* event, const pybind11::args& args);
  [[nodiscard]] std::string str() const;
  [[nodiscard]] uint32_t hash() const;

private:
  ObjectWrapper(std::shared_ptr<Object> obj, RunApi* api);
  ObjectWrapper(std::shared_ptr<NativeObject> obj, RunApi* api);

private:
  std::shared_ptr<Object> obj_;
  std::shared_ptr<NativeObject> native_;
  ::sen::impl::WorkQueue* workQueue_;
  RunApi* api_;
};

}  // namespace sen::components::py

#endif  // SEN_COMPONENTS_PY_SRC_OBJECT_WRAPPER_H
