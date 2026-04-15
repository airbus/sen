// === object_list_wrapper.cpp =========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "object_list_wrapper.h"

// component
#include "component_api.h"
#include "object_wrapper.h"

// sen
#include "sen/core/obj/object_provider.h"
#include "sen/core/obj/object_source.h"

// pybind11
#include <pybind11/attr.h>
#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

// std
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>

namespace sen::components::py
{

ObjectListWrapper::ObjectListWrapper(std::shared_ptr<ObjectSource> source,
                                     std::shared_ptr<ObjectProvider> provider,
                                     std::string providerName,
                                     RunApi* api)
  : source_(std::move(source)), provider_(std::move(provider)), providerName_(std::move(providerName)), api_(api)
{
  std::ignore = list_.onAdded(
    [this](const auto& addedObjects)
    {
      for (auto obj: addedObjects)
      {
        wrappers_.push_back(ObjectWrapper::makeGeneric(obj->shared_from_this(), api_));
        callBack(wrappers_.back(), onAddedCallback_);
      }
    });

  std::ignore = list_.onRemoved(
    [this](const auto& removedObjects)
    {
      for (auto obj: removedObjects)
      {
        for (auto wrapperItr = wrappers_.begin(); wrapperItr != wrappers_.end(); ++wrapperItr)
        {
          if (wrapperItr->get()->getId() == obj->getId())
          {
            const auto& wrapper = *wrapperItr;
            wrappers_.erase(wrapperItr);
            callBack(wrapper, onRemovedCallback_);
            break;
          }
        }
      }
    });

  provider_->addListener(&list_, true);
}

ObjectListWrapper::~ObjectListWrapper()
{
  provider_->removeListener(&list_, true);
  source_->removeNamedProvider(providerName_);
}

void ObjectListWrapper::callBack(const ObjectWrapper& wrapper, pybind11::object& func) const
{
  if (!func)
  {
    return;
  }

  try
  {
    func(wrapper);
  }
  catch (pybind11::error_already_set& err)
  {
    getLogger()->error("error detected in Python callback: {}", err.what());
    err.discard_as_unraisable(__func__);
  }
  catch (const std::exception& err)
  {
    getLogger()->error("unknown error detected in Python callback: {}", err.what());
  }
}

void ObjectListWrapper::definePythonApi(pybind11::module_& self)
{
  // dependent types
  ObjectWrapper::definePythonApi(self);

  // our class
  pybind11::class_<ObjectListWrapper> cl(self, "ObjectList");

  using T = typename WrapperList::value_type;
  using SizeType = typename WrapperList::size_type;
  using DiffType = typename WrapperList::difference_type;
  using ItType = typename WrapperList::iterator;

  cl.def("onAdded",
         [](ObjectListWrapper& theWrapper, const pybind11::object& callback)
         { theWrapper.onAddedCallback_ = callback; });

  cl.def("onRemoved",
         [](ObjectListWrapper& theWrapper, const pybind11::object& callback)
         { theWrapper.onRemovedCallback_ = callback; });

  cl.def(
    "waitUntilEmpty",
    [](ObjectListWrapper& theWrapper)
    {
      return theWrapper.api_->waitUntilCpp([&theWrapper]() { return theWrapper.list_.getUntypedObjects().empty(); });
    });

  cl.def(
    "waitUntilNotEmpty",
    [](ObjectListWrapper& theWrapper)
    {
      return theWrapper.api_->waitUntilCpp([&theWrapper]() { return !theWrapper.list_.getUntypedObjects().empty(); });
    });

  cl.def("waitUntilSizeIs",
         [](ObjectListWrapper& theWrapper, uint32_t count)
         {
           return theWrapper.api_->waitUntilCpp([&theWrapper, count]()
                                                { return theWrapper.list_.getUntypedObjects().size() == count; });
         });

  cl.def(
    "__getitem__",
    [](ObjectListWrapper& v, DiffType i) -> T&
    {
      if (i < 0)
      {
        i += v.wrappers_.size();  // NOLINT
      }
      if (i < 0 || static_cast<SizeType>(i) >= v.wrappers_.size())
      {
        throw pybind11::index_error();
      }
      return v.wrappers_[static_cast<SizeType>(i)];
    },
    pybind11::return_value_policy::reference_internal  // ref + keepalive
  );

  cl.def(
    "__iter__",
    [](ObjectListWrapper& v)
    {
      return pybind11::make_iterator<pybind11::return_value_policy::reference_internal, ItType, ItType, T&>(
        v.wrappers_.begin(), v.wrappers_.end());
    },
    pybind11::keep_alive<0, 1>()  // Essential: keep list alive while iterator exists
  );

  cl.def("__bool__", [](const ObjectListWrapper& v) { return !v.wrappers_.empty(); });
  cl.def("__len__", [](const ObjectListWrapper& v) { return v.wrappers_.size(); });
  cl.def("__repr__",
         [](ObjectListWrapper& v)
         {
           std::ostringstream s;
           s << "[";

           for (std::size_t i = 0; i < v.wrappers_.size(); ++i)
           {
             s << v.wrappers_[i].get()->getName();
             if (i != v.wrappers_.size() - 1)
             {
               s << ", ";
             }
           }
           s << "]";
           return s.str();
         });
}

}  // namespace sen::components::py
