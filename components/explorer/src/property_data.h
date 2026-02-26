// === property_data.h =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_PROPERTY_DATA_H
#define SEN_COMPONENTS_EXPLORER_SRC_PROPERTY_DATA_H

// editable printer
#include "editable_printer_maker.h"

// sen
#include "sen/core/meta/property.h"
#include "sen/core/meta/type.h"
#include "sen/core/obj/detail/work_queue.h"

// std
#include <unordered_map>

struct DataPoint
{
  sen::TimeStamp time;
  float64_t value;
};

class ObjectState;
class Plot;
using ObservedDataMap = std::unordered_map<uint32_t, std::vector<Plot*>>;

struct PropStateDrag
{
  uint32_t elementHash = 0;
  ObjectState* state = nullptr;
  char elementLabel[128] {};

  [[nodiscard]] static const char* getTypeString() noexcept;
};

using PrinterFunc = std::function<void(const sen::Var&, uint32_t, const std::string&, ObjectState*)>;
using ReporterFunc = std::function<void(const sen::Var&, uint32_t, const sen::TimeStamp&, ObservedDataMap&)>;

using PrinterRegistry = std::unordered_map<sen::ConstTypeHandle<>, PrinterFunc>;
using ReporterRegistry = std::unordered_map<sen::ConstTypeHandle<>, ReporterFunc>;

class PropertyData
{
  SEN_NOCOPY_NOMOVE(PropertyData)

public:
  PropertyData(const sen::Property* prop,
               ObjectState* owner,
               sen::impl::WorkQueue* queue,
               sen::Var initialValue,
               PrinterRegistry& printerRegistry,
               ReporterRegistry& reporterRegistry);
  ~PropertyData();

private:
  friend class ObjectState;
  void changed(ObservedDataMap& observedData);
  void update();

private:
  ObjectState* owner_;
  const sen::Property* prop_ = nullptr;
  uint32_t hash_;
  sen::Var value_;
  uint8_t age_ = 0U;
  std::string prefix_;
  PrinterFunc printer_;
  EditablePrinterFunc printerEditable_;
  ReporterFunc reporter_;
  EditablePrinterMaker::PropertiesStateMap propertiesStateMap_;
};

#endif  // SEN_COMPONENTS_EXPLORER_SRC_PROPERTY_DATA_H
