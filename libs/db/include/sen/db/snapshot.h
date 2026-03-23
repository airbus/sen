// === snapshot.h ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_DB_SNAPSHOT_H
#define SEN_DB_SNAPSHOT_H

#include "sen/core/meta/type.h"
#include "sen/core/obj/object.h"

namespace sen::db
{

class Input;

/// Immutable snapshot of the full property state of a recorded object at a specific point in time.
/// Snapshots are produced by the `sen::db::Input` cursor and give random-access to property values
/// either as type-erased variants (`Var`) or as raw serialised bytes.
/// \ingroup db
class Snapshot
{
  SEN_COPY_MOVE(Snapshot)

public:
  ~Snapshot() = default;

public:
  /// @return Unique numeric identifier of the recorded object.
  [[nodiscard]] ObjectId getObjectId() const noexcept;

  /// @return Name of the recorded object as published to the bus.
  [[nodiscard]] const std::string& getName() const noexcept;

  /// @return Name of the session on which the object was published.
  [[nodiscard]] const std::string& getSessionName() const noexcept;

  /// @return Name of the bus on which the object was published.
  [[nodiscard]] const std::string& getBusName() const noexcept;

  /// @return Type handle for the reflected `ClassType` of the recorded object.
  [[nodiscard]] const ConstTypeHandle<ClassType>& getType() const noexcept { return type_; }

public:
  /// Returns the value of a property as a type-erased variant.
  /// The result is lazily deserialised and cached on first access.
  /// @param property  Descriptor of the property to read (must belong to the object's class).
  /// @return Reference to the cached Var; the reference is valid for the lifetime of this Snapshot.
  /// @throws std::out_of_range if @p property is not part of the recorded object's type.
  [[nodiscard]] const Var& getPropertyAsVariant(const Property* property) const;

  /// Returns the value of a property as a raw serialised byte span.
  /// Useful for zero-copy forwarding without deserialising to a Var.
  /// @param property  Descriptor of the property to read (must belong to the object's class).
  /// @return Non-owning span into the internal buffer; valid for the lifetime of this Snapshot.
  /// @throws std::out_of_range if @p property is not part of the recorded object's type.
  [[nodiscard]] Span<const uint8_t> getPropertyAsBuffer(const Property* property) const;

  /// Returns a span over the entire serialised property buffer for this snapshot.
  /// The layout matches the Sen wire format used by the recorder component.
  /// @return Non-owning span into the internal buffer; valid for the lifetime of this Snapshot.
  [[nodiscard]] Span<const uint8_t> getAllPropertiesBuffer() const noexcept;

private:
  friend class Input;
  Snapshot(ObjectId objectId,
           std::string objectName,
           std::string sessionName,
           std::string busName,
           ConstTypeHandle<ClassType> type,
           std::vector<uint8_t> buffer);

private:
  void extractBuffersIfNeeded() const;

private:
  ObjectId objectId_;
  std::string objectName_;
  std::string sessionName_;
  std::string busName_;
  ConstTypeHandle<ClassType> type_;
  std::vector<uint8_t> buffer_;
  mutable bool buffersExtracted_ = false;
  mutable std::unordered_map<const Property*, Var> varCache_;
  mutable std::unordered_map<MemberHash, Span<const uint8_t>> bufferCache_;
};

}  // namespace sen::db

#endif  // SEN_DB_SNAPSHOT_H
