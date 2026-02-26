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

/// Represents the full state of an object at a point in time.
/// \ingroup db
class Snapshot
{
  SEN_COPY_MOVE(Snapshot)

public:
  ~Snapshot() = default;

public:
  /// The ID of the object.
  [[nodiscard]] ObjectId getObjectId() const noexcept;

  /// The name of the object.
  [[nodiscard]] const std::string& getName() const noexcept;

  /// The session where the object was published.
  [[nodiscard]] const std::string& getSessionName() const noexcept;

  /// The bus where the object was published.
  [[nodiscard]] const std::string& getBusName() const noexcept;

  /// The class of the object.
  [[nodiscard]] const ConstTypeHandle<ClassType>& getType() const noexcept { return type_; }

public:
  /// Get the value of a property as a variant.
  [[nodiscard]] const Var& getPropertyAsVariant(const Property* property) const;

  /// Get the value of a property as a serialized buffer.
  [[nodiscard]] Span<const uint8_t> getPropertyAsBuffer(const Property* property) const;

  /// Get the buffer containing all the serialized properties.
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
