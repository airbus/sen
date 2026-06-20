// === module.cpp ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "type_casters.h"  // registers pybind11 caster for sen::ObjectId
#include "type_specs.h"
#include "variant_conversion.h"

// sen
#include "sen/core/base/span.h"
#include "sen/core/meta/class_type.h"
#include "sen/core/meta/custom_type.h"
#include "sen/core/meta/type_registry.h"
#include "sen/core/meta/var.h"
#include "sen/core/obj/object.h"
#include "sen/db/annotation.h"
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
#include <pybind11/chrono.h>  // std::chrono <-> datetime.timedelta for TimeStamp/Duration casts
#include <pybind11/detail/common.h>
#include <pybind11/pybind11.h>
#include <pybind11/pytypes.h>
#include <pybind11/stl.h>  // std::optional / std::vector / std::variant casters used below

// std
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using sen::CustomType;
using sen::CustomTypeRegistry;
using sen::ObjectId;
using sen::db::Annotation;
using sen::db::AnnotationCursor;
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

// PYBIND11_MAKE_OPAQUE must precede any pybind11 template instantiation that mentions
// these types (otherwise the holder type gets baked in as auto-converting). Leave these
// at namespace scope above the anonymous namespace; do not move into a class body.
PYBIND11_MAKE_OPAQUE(sen::Span<const Snapshot>)
PYBIND11_MAKE_OPAQUE(sen::Span<const ObjectIndexDef>)
PYBIND11_MAKE_OPAQUE(sen::Span<const KeyframeIndex>)

namespace
{

class TypeRegistryPy
{
public:
  explicit TypeRegistryPy(const CustomTypeRegistry& reg): snapshot_(reg.getAll()) {}

  [[nodiscard]] const CustomTypeRegistry::CustomTypeMap& snapshot() const noexcept { return snapshot_; }

private:
  CustomTypeRegistry::CustomTypeMap snapshot_;
};

void clearLookupPropertyCache();

// Member order matters: `input_` is destroyed first, then `types_`, so the Input never
// outlives the registry it references.
class InputPy
{
  SEN_NOCOPY_NOMOVE(InputPy)

public:
  explicit InputPy(const std::string& path)
    : types_(std::make_unique<CustomTypeRegistry>()), input_(std::make_unique<Input>(path, *types_))
  {
  }

  // Closing this Input invalidates the static lookupProperty cache: the ClassType pointers
  // it cached now belong to freed memory and an allocator could recycle the addresses for
  // a different Input. See the cache definition for the full rationale.
  ~InputPy() { clearLookupPropertyCache(); }

  [[nodiscard]] Input& input() noexcept { return *input_; }
  [[nodiscard]] const Input& input() const noexcept { return *input_; }

  // Lazily-built per-Input snapshot of the type registry. `CustomTypeRegistry::getAll()`
  // returns by value; building the TypeRegistryPy snapshot on every getTypes() call would
  // copy the whole `unordered_map<string, ConstTypeHandle<CustomType>>` each time. The
  // registry inside Input is immutable post-open (Input populates it during construction;
  // nothing mutates it after), so a single cached snapshot is safe.
  [[nodiscard]] const TypeRegistryPy& cachedTypes()
  {
    if (!cachedTypes_)
    {
      cachedTypes_ = std::make_unique<TypeRegistryPy>(input_->getTypes());
    }
    return *cachedTypes_;
  }

private:
  std::unique_ptr<CustomTypeRegistry> types_;
  std::unique_ptr<Input> input_;
  std::unique_ptr<TypeRegistryPy> cachedTypes_;
};

// Per-ClassType index: name list (ready to return as a Python list) + name -> Property*
// dispatch map. Keys are `ClassType*`; the inner name-list owns its strings and the
// `string_view` keys in `byName` reference those owned strings. Both are valid while the
// owning TypeRegistry is alive. When an Input closes, those ClassTypes are deallocated
// and the allocator is free to reuse the addresses -- a subsequent cache hit on a
// recycled address would silently return the wrong index. `InputPy::~InputPy` calls
// `clearLookupPropertyCache()` so no stale `ClassType*` keys can outlive their owners.
// The clear is global (not per-Input); for the typical single-Input post-processing
// workflow this is invisible. Multi-Input scripts rebuild on first access after a close.
struct ClassTypeIndex
{
  std::vector<std::string> names;                                     // owned name storage
  std::unordered_map<std::string_view, const sen::Property*> byName;  // views into names
};

[[nodiscard]] std::unordered_map<const sen::ClassType*, ClassTypeIndex>& lookupPropertyCache()
{
  static std::unordered_map<const sen::ClassType*, ClassTypeIndex> cache;
  return cache;
}

void clearLookupPropertyCache() { lookupPropertyCache().clear(); }

[[nodiscard]] const ClassTypeIndex& lookupClassTypeIndex(const sen::ClassType& classType)
{
  auto& cache = lookupPropertyCache();
  auto it = cache.find(&classType);
  if (it == cache.end())
  {
    ClassTypeIndex idx;
    auto props = classType.getProperties(sen::ClassType::SearchMode::includeParents);
    idx.names.reserve(props.size());
    for (const auto& prop: props)
    {
      idx.names.emplace_back(prop->getName());
    }
    // Build byName *after* names is fully populated so string_views point into stable
    // storage (reserve+emplace_back is stable; later mutation would invalidate).
    for (std::size_t i = 0; i < props.size(); ++i)
    {
      idx.byName.emplace(std::string_view {idx.names[i]}, props[i].get());
    }
    it = cache.emplace(&classType, std::move(idx)).first;
  }
  return it->second;
}

// O(1) dispatch for Snapshot's six built-in attributes (`name`, `busName`, `id`,
// `sessionName`, `className`, `propertyNames`). Falls through to the per-ClassType
// property index. Built-ins are checked first because Sen's property namespace doesn't
// collide with these reserved names by construction, but reordering would change
// semantics if it ever did -- safer to keep built-ins canonical.
using SnapshotAttrHandler = pybind11::object (*)(const Snapshot&);
[[nodiscard]] const std::unordered_map<std::string_view, SnapshotAttrHandler>& snapshotAttrTable()
{
  static const std::unordered_map<std::string_view, SnapshotAttrHandler> table = {
    {"name", [](const Snapshot& s) -> pybind11::object { return pybind11::cast(s.getName()); }},
    {"busName", [](const Snapshot& s) -> pybind11::object { return pybind11::cast(s.getBusName()); }},
    {"objectId", [](const Snapshot& s) -> pybind11::object { return pybind11::cast(s.getObjectId()); }},
    {"sessionName", [](const Snapshot& s) -> pybind11::object { return pybind11::cast(s.getSessionName()); }},
    {"className",
     [](const Snapshot& s) -> pybind11::object { return pybind11::cast(s.getType()->getQualifiedName()); }},
    {"propertyNames",
     [](const Snapshot& s) -> pybind11::object
     { return pybind11::cast(lookupClassTypeIndex(*s.getType().type()).names); }},
  };
  return table;
}

[[nodiscard]] pybind11::object getAttribute(const Snapshot& snapshot, const std::string& name)
{
  const auto& builtins = snapshotAttrTable();
  if (auto it = builtins.find(name); it != builtins.end())
  {
    return it->second(snapshot);
  }

  const auto& idx = lookupClassTypeIndex(*snapshot.getType().type());
  if (auto pit = idx.byName.find(name); pit != idx.byName.end())
  {
    const auto* prop = pit->second;
    return toPython(snapshot.getPropertyAsVariant(prop), prop->getType().type());
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
          i += static_cast<DiffType>(v.size());
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
  m.doc() = "Read access to Sen recordings (see docs/users_guide/db_python_bindings.md for the full reference).";

  pybind11::class_<Summary>(m, "Summary", "Headline metrics for a recording.")
    .def_property_readonly(
      "firstTime",
      [](const Summary& self) { return self.firstTime.sinceEpoch().toChrono(); },
      "timedelta since epoch of the first sample")
    .def_property_readonly(
      "lastTime",
      [](const Summary& self) { return self.lastTime.sinceEpoch().toChrono(); },
      "timedelta since epoch of the last sample")
    .def_readonly("keyframeCount", &Summary::keyframeCount)
    .def_readonly("objectCount", &Summary::objectCount)
    .def_readonly("typeCount", &Summary::typeCount)
    .def_readonly("annotationCount", &Summary::annotationCount)
    .def_readonly("indexedObjectCount", &Summary::indexedObjectCount);

  pybind11::class_<End>(m, "End", "Sentinel marking the end of a cursor.");  // NOLINT

  pybind11::class_<PropertyChange>(m, "PropertyChange", "Property update event.")
    .def_property_readonly("objectId", &PropertyChange::getObjectId)
    .def_property_readonly(
      "value",
      [](const PropertyChange& self)
      { return toPython(self.getValueAsVariant(), self.getProperty()->getType().type()); },
      "the new value, converted to a Python value")
    .def_property_readonly(
      "name", [](const PropertyChange& self) { return self.getProperty()->getName(); }, "property name");

  pybind11::class_<Event>(m, "Event", "Object event emission.")
    .def_property_readonly("objectId", &Event::getObjectId)
    .def_property_readonly(
      "name", [](const Event& self) { return self.getEvent()->getName(); }, "event name")
    .def_property_readonly(
      "args",
      [](const Event& self) -> pybind11::list
      {
        pybind11::list result;
        const auto& vars = self.getArgsAsVariants();
        const auto metaArgs = self.getEvent()->getArgs();
        const std::size_t typed = std::min(vars.size(), metaArgs.size());
        for (std::size_t i = 0; i < typed; ++i)
        {
          result.append(toPython(vars[i], metaArgs[i].type.type()));
        }
        for (std::size_t i = typed; i < vars.size(); ++i)
        {
          result.append(toPython(vars[i]));
        }
        return result;
      },
      "list of converted argument values, one per declared argument");

  pybind11::class_<Snapshot>(
    m,
    "Snapshot",
    "Object state at a point in time. Read fields via attribute access: 'name', 'busName', "
    "'sessionName', 'id', 'className', 'propertyNames', or any property name on the object's class.")
    .def("__getattr__", [](const Snapshot& self, const std::string& name) { return getAttribute(self, name); });

  pybind11::class_<Creation>(
    m, "Creation", "Object creation event. Delegates attribute lookup to the embedded Snapshot.")
    .def("__getattr__",
         [](const Creation& self, const std::string& name) { return getAttribute(self.getSnapshot(), name); });

  pybind11::class_<Deletion>(m, "Deletion", "Object deletion event.")
    .def_property_readonly("objectId", &Deletion::getObjectId);

  defineSequence<sen::Span<const Snapshot>>(m, "SnapshotList");

  pybind11::class_<Keyframe>(m, "Keyframe", "Full-state snapshot of every recorded object.")
    .def_property_readonly("snapshots", &Keyframe::getSnapshots, pybind11::keep_alive<0, 1>());

  pybind11::class_<DataCursor::Entry>(m, "DataEntry", "One entry in the data cursor: timestamp + payload.")
    .def_property_readonly(
      "time", [](const DataCursor::Entry& self) { return self.time.sinceEpoch().toChrono(); }, "timedelta since epoch")
    // Copy semantics: the Python payload owns its data, so it (and any references the user
    // holds into its nested attributes, like Keyframe.snapshots' Span) survives advance().
    // The alternative (aliasing the cursor's variant slot) made keep_alive<0,1> a false
    // promise -- the Python wrapper outlived the C++ storage it referenced. Costs one
    // payload copy per read; bindings are aimed at post-processing, not hot streaming.
    .def_property_readonly(
      "payload",
      [](const DataCursor::Entry& self) -> pybind11::object
      {
        return std::visit(
          [](const auto& alt) -> pybind11::object
          {
            using A = std::decay_t<decltype(alt)>;
            if constexpr (std::is_same_v<A, std::monostate>)
            {
              return pybind11::none();
            }
            else
            {
              return pybind11::cast(alt, pybind11::return_value_policy::copy);
            }
          },
          self.payload);
      },
      "active variant alternative: PropertyChange / Event / Keyframe / Creation / "
      "Deletion / End, or None before the first advance()");

  pybind11::class_<DataCursor>(m, "DataCursor", "Forward iterator over the runtime data in a recording.")
    .def_property_readonly("atEnd", &DataCursor::atEnd, "True once the cursor has passed the last entry")
    .def_property_readonly("atStart", &DataCursor::atBegining, "True before the first advance()")
    .def_property_readonly("entry", &DataCursor::get, "current DataEntry")
    .def("advance", &DataCursor::operator++, "move to the next entry");

  pybind11::class_<ObjectIndexDef>(m, "ObjectIndexDef", "Index entry for one object: identity + class.")
    .def_property_readonly(
      "type", [](const ObjectIndexDef& self) { return self.type->getQualifiedName(); }, "qualified class name")
    .def_readonly("objectId", &ObjectIndexDef::objectId)
    .def_readonly("name", &ObjectIndexDef::name)
    .def_readonly("session", &ObjectIndexDef::session)
    .def_readonly("bus", &ObjectIndexDef::bus)
    .def_readonly("indexId", &ObjectIndexDef::indexId);

  pybind11::class_<KeyframeIndex>(m, "KeyframeIndex", "Index entry for one keyframe: byte offset + time.")
    .def_readonly("offset", &KeyframeIndex::offset)
    .def_property_readonly(
      "time", [](const KeyframeIndex& self) { return self.time.sinceEpoch().toChrono(); }, "timedelta since epoch");

  defineSequence<sen::Span<const ObjectIndexDef>>(m, "ObjectIndexDefList");
  defineSequence<sen::Span<const KeyframeIndex>>(m, "KeyframeIndexList");

  pybind11::class_<TypeRegistryPy>(
    m, "TypeRegistry", "Snapshot of every class, struct, variant, alias, enum, and quantity in the recording.")
    .def("__len__", [](const TypeRegistryPy& self) { return self.snapshot().size(); })
    .def("__contains__",
         [](const TypeRegistryPy& self, const std::string& name)
         { return self.snapshot().find(name) != self.snapshot().end(); })
    .def_property_readonly(
      "classNames",
      [](const TypeRegistryPy& self)
      {
        std::vector<std::string> names;
        names.reserve(self.snapshot().size());
        for (const auto& [name, _]: self.snapshot())
        {
          names.push_back(name);
        }
        return names;
      },
      "list of qualified type names")
    .def(
      "getTypeSpec",
      [](const TypeRegistryPy& self, const std::string& name) -> pybind11::object
      {
        const auto it = self.snapshot().find(name);
        if (it == self.snapshot().end())
        {
          return pybind11::none();
        }
        return sen::db::python::customTypeToPython(*it->second);
      },
      pybind11::arg("qualifiedName"),
      "dict describing the type (kernel external CustomTypeSpec shape), or None if absent")
    .def(
      "getAllTypeSpecs",
      [](const TypeRegistryPy& self)
      {
        pybind11::dict result;
        for (const auto& [name, handle]: self.snapshot())
        {
          result[pybind11::str(name)] = sen::db::python::customTypeToPython(*handle);
        }
        return result;
      },
      "dict keyed by qualified name; values are the same shape getTypeSpec returns");

  pybind11::class_<Annotation>(m, "Annotation", "User-defined annotation attached to a timestamp.")
    .def_property_readonly(
      "type",
      [](const Annotation& self) -> pybind11::object
      {
        const auto* type = self.getType().type();
        if (const auto* custom = type->asCustomType())
        {
          return pybind11::cast(std::string {custom->getQualifiedName()});
        }
        return pybind11::cast(std::string {type->getName()});
      },
      "qualified name for custom types; basic type name (e.g. f64) for built-ins")
    .def_property_readonly(
      "value",
      [](const Annotation& self) { return toPython(self.getValueAsVariant(), self.getType().type()); },
      "converted Python value");

  pybind11::class_<AnnotationCursor::Entry>(
    m, "AnnotationEntry", "One entry in the annotation cursor: timestamp + payload.")
    .def_property_readonly(
      "time",
      [](const AnnotationCursor::Entry& self) { return self.time.sinceEpoch().toChrono(); },
      "timedelta since epoch")
    .def_property_readonly(
      "payload",
      [](const AnnotationCursor::Entry& self) -> pybind11::object
      {
        return std::visit(
          [](const auto& alt) -> pybind11::object
          {
            using A = std::decay_t<decltype(alt)>;
            if constexpr (std::is_same_v<A, std::monostate>)
            {
              return pybind11::none();
            }
            else
            {
              return pybind11::cast(alt);
            }
          },
          self.payload);
      },
      "Annotation, End, or None before the first advance()");

  pybind11::class_<AnnotationCursor>(m, "AnnotationCursor", "Forward iterator over the annotations in a recording.")
    .def_property_readonly("atEnd", &AnnotationCursor::atEnd)
    .def_property_readonly("atStart", &AnnotationCursor::atBegining)
    .def_property_readonly("entry", &AnnotationCursor::get)
    .def("advance", &AnnotationCursor::operator++);

  pybind11::class_<InputPy>(m, "Input", "Open recording handle.")
    .def(pybind11::init<const std::string&>(),
         pybind11::arg("path"),
         "open the recording directory at the given filesystem path")
    .def_property_readonly("path", [](const InputPy& self) { return self.input().getPath().string(); })
    .def_property_readonly(
      "summary",
      [](const InputPy& self) -> const Summary& { return self.input().getSummary(); },
      pybind11::return_value_policy::reference_internal)
    .def(
      "getTypes",
      [](InputPy& self) -> const TypeRegistryPy& { return self.cachedTypes(); },
      pybind11::return_value_policy::reference_internal,
      "TypeRegistry snapshot for every type in the recording (cached per Input)")
    .def(
      "begin",
      [](InputPy& self) { return self.input().begin(); },
      pybind11::keep_alive<0, 1>(),
      "DataCursor starting at the first runtime entry")
    .def(
      "annotationsBegin",
      [](InputPy& self) { return self.input().annotationsBegin(); },
      pybind11::keep_alive<0, 1>(),
      "AnnotationCursor starting at the first annotation")
    .def(
      "at",
      [](InputPy& self, const KeyframeIndex& k) { return self.input().at(k); },
      pybind11::arg("keyframeIndex"),
      pybind11::keep_alive<0, 1>(),
      "DataCursor starting at the given keyframe")
    .def(
      "makeCursor",
      [](InputPy& self, const ObjectIndexDef& d) { return self.input().makeCursor(d); },
      pybind11::arg("objectIndexDef"),
      pybind11::keep_alive<0, 1>(),
      "DataCursor iterating only entries for the given indexed object")
    .def(
      "getKeyframeIndex",
      [](InputPy& self, sen::TimeStamp t) { return self.input().getKeyframeIndex(t); },
      pybind11::arg("time"),
      "closest KeyframeIndex at or before the given time, or None")
    .def(
      "getObjectIndexDefinitions",
      [](InputPy& self) { return self.input().getObjectIndexDefinitions(); },
      pybind11::keep_alive<0, 1>(),
      "every indexed object's identity")
    .def(
      "getAllKeyframeIndexes",
      [](InputPy& self) { return self.input().getAllKeyframeIndexes(); },
      pybind11::keep_alive<0, 1>(),
      "every keyframe's offset + time");
}
