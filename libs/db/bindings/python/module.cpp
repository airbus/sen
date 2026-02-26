// === module.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "variant_conversion.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"
#include "sen/db/creation.h"
#include "sen/db/deletion.h"
#include "sen/db/event.h"
#include "sen/db/input.h"
#include "sen/db/keyframe.h"
#include "sen/db/property_change.h"
#include "sen/db/snapshot.h"

// generated code
#include "stl/sen/db/basic_types.stl.h"

// pybind
#include <pybind11/attr.h>
#include <pybind11/cast.h>
#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>

// std
#include <memory>
#include <string>
#include <vector>

using sen::ObjectId;
using sen::db::Creation;
using sen::db::DataCursor;
using sen::db::Deletion;
using sen::db::End;
using sen::db::Event;
using sen::db::Input;
using sen::db::Keyframe;
using sen::db::KeyframeIndex;
using sen::db::ObjectIndexDef;
using sen::db::PropertyChange;
using sen::db::Snapshot;
using sen::db::Summary;

PYBIND11_MAKE_OPAQUE(std::vector<sen::Var>)
PYBIND11_MAKE_OPAQUE(sen::Span<const Snapshot>)
PYBIND11_MAKE_OPAQUE(sen::Span<const ObjectIndexDef>)
PYBIND11_MAKE_OPAQUE(sen::Span<const KeyframeIndex>)

namespace
{

[[nodiscard]] pybind11::object getAttribute(const Snapshot& snapshot, const std::string& name)
{
  if (name == "name")
  {
    return pybind11::cast(snapshot.getName());
  }
  if (name == "busName")
  {
    return pybind11::cast(snapshot.getBusName());
  }
  if (name == "id")
  {
    return pybind11::cast(snapshot.getObjectId());
  }
  if (name == "sessionName")
  {
    return pybind11::cast(snapshot.getSessionName());
  }
  if (name == "className")
  {
    return pybind11::cast(snapshot.getType()->getQualifiedName());
  }

  const auto* meta = snapshot.getType().type();

  // check if the user requests the whole vector of property names
  if (name == "propertyNames")
  {
    std::vector<std::string> propertyNames;
    auto properties = meta->getProperties(sen::ClassType::SearchMode::includeParents);
    propertyNames.reserve(properties.size());

    for (const auto& prop: properties)
    {
      propertyNames.emplace_back(prop->getName());
    }
    return pybind11::cast(propertyNames);
  }

  // check if we are accessing a property
  if (const auto* prop = meta->searchPropertyByName(name))
  {
    return toPython(snapshot.getPropertyAsVariant(prop));
  }

  std::string err;
  err.append("invalid member '");
  err.append(name);
  err.append("' for object ");
  err.append(snapshot.getName());
  err.append(" of class ");
  err.append(snapshot.getType()->getQualifiedName());
  throw pybind11::attribute_error(err);
}

template <typename Container>
void defineSequence(pybind11::module& m, const char* name)
{
  using T = typename Container::value_type;
  using SizeType = typename std::vector<T>::size_type;
  using DiffType = typename std::vector<T>::difference_type;

  pybind11::class_<Container>(m, name)
    .def("__len__", [](const Container& v) { return v.size(); })
    .def(
      "__iter__",
      [](Container& v) { return pybind11::make_iterator(v.begin(), v.end()); },
      pybind11::keep_alive<0, 1>())
    .def(
      "__getitem__",
      [](Container& v, DiffType i) -> const T&
      {
        if (i < 0)
        {
          i += v.size();  // NOLINT
        }
        if (i < 0 || static_cast<SizeType>(i) >= v.size())
        {
          throw pybind11::index_error();
        }
        return v[static_cast<SizeType>(i)];
      },
      pybind11::return_value_policy::reference_internal  // ref + keepalive
    );
}

}  // namespace

PYBIND11_MODULE(sen_db_python, m)
{
  static sen::CustomTypeRegistry types;

  pybind11::class_<Summary>(m, "Summary")
    .def_property_readonly("firstTime", [](const Summary& self) { return self.firstTime.sinceEpoch().toChrono(); })
    .def_property_readonly("lastTime", [](const Summary& self) { return self.lastTime.sinceEpoch().toChrono(); })
    .def_readonly("keyframeCount", &Summary::keyframeCount)
    .def_readonly("objectCount", &Summary::objectCount)
    .def_readonly("typeCount", &Summary::typeCount)
    .def_readonly("annotationCount", &Summary::annotationCount)
    .def_readonly("indexedObjectCount", &Summary::indexedObjectCount);

  pybind11::class_<End>(m, "End");  // NOLINT

  pybind11::class_<PropertyChange>(m, "PropertyChange")
    .def_property_readonly("objectId", &PropertyChange::getObjectId)
    .def_property_readonly("value", [](const PropertyChange& self) { return toPython(self.getValueAsVariant()); })
    .def_property_readonly("name", [](const PropertyChange& self) { return self.getProperty()->getName(); });

  defineSequence<std::vector<sen::Var>>(m, "VarList");

  pybind11::class_<Event>(m, "Event")
    .def_property_readonly("objectId", &Event::getObjectId)
    .def_property_readonly("name", [](const Event& self) { return self.getEvent()->getName(); })
    .def_property_readonly("args", [](const Event& self) { return self.getArgsAsVariants(); });

  pybind11::class_<Snapshot>(m, "Snapshot")
    .def("__getattribute__", [](const Snapshot& self, const std::string& name) { return getAttribute(self, name); });

  pybind11::class_<Creation>(m, "Creation")
    .def("__getattribute__",
         [](const Creation& self, const std::string& name) { return getAttribute(self.getSnapshot(), name); });

  pybind11::class_<Deletion>(m, "Deletion").def_property_readonly("objectId", &Deletion::getObjectId);

  defineSequence<sen::Span<const Snapshot>>(m, "SnapshotList");

  pybind11::class_<Keyframe>(m, "Keyframe").def_property_readonly("snapshots", &Keyframe::getSnapshots);

  pybind11::class_<DataCursor::Payload> dataPayload(m, "DataPayload");

  pybind11::class_<DataCursor::Entry>(m, "DataEntry")
    .def_property_readonly("time", [](const DataCursor::Entry& self) { return self.time.sinceEpoch().toChrono(); })
    .def_readonly("payload", &DataCursor::Entry::payload);

  pybind11::class_<DataCursor>(m, "DataCursor")
    .def_property_readonly("atEnd", &DataCursor::atEnd)
    .def_property_readonly("atStart", &DataCursor::atBegining)
    .def_property_readonly("entry", &DataCursor::get)
    .def("advance", &DataCursor::operator++);

  pybind11::class_<ObjectIndexDef>(m, "ObjectIndexDef")
    .def_property_readonly("type", [](const ObjectIndexDef& self) { return self.type->getQualifiedName(); })
    .def_readonly("objectId", &ObjectIndexDef::objectId)
    .def_readonly("name", &ObjectIndexDef::name)
    .def_readonly("session", &ObjectIndexDef::session)
    .def_readonly("bus", &ObjectIndexDef::bus)
    .def_readonly("indexId", &ObjectIndexDef::indexId);

  pybind11::class_<KeyframeIndex>(m, "KeyframeIndex")
    .def_readonly("offset", &KeyframeIndex::offset)
    .def_property_readonly("time", [](const KeyframeIndex& self) { return self.time.sinceEpoch().toChrono(); });

  defineSequence<sen::Span<const ObjectIndexDef>>(m, "ObjectIndexDefList");
  defineSequence<sen::Span<const KeyframeIndex>>(m, "KeyframeIndexList");

  pybind11::class_<Input>(m, "Input")
    .def(pybind11::init([](std::string file) { return std::make_unique<Input>(file, types); }))
    .def_property_readonly("path", [](const Input& self) { return self.getPath().string(); })
    .def_property_readonly("summary", &Input::getSummary)
    .def("begin", &Input::begin)
    .def("at", &Input::at)
    .def("makeCursor", &Input::makeCursor)
    .def("getKeyframeIndex", &Input::getKeyframeIndex)
    .def("getObjectIndexDefinitions", &Input::getObjectIndexDefinitions)
    .def("getAllKeyframeIndexes", &Input::getAllKeyframeIndexes);
}
