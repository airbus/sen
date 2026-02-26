// === object_state.h ==================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_COMPONENTS_EXPLORER_SRC_OBJECT_STATE_H
#define SEN_COMPONENTS_EXPLORER_SRC_OBJECT_STATE_H

// printers
#include "method_printer.h"
#include "property_data.h"

// sen
#include "sen/core/obj/object.h"
#include "sen/kernel/component_api.h"

class Plot;
class EventExplorer;

class ObjectState: public std::enable_shared_from_this<ObjectState>
{
  SEN_NOCOPY_NOMOVE(ObjectState)

public:
  [[nodiscard]] static std::shared_ptr<ObjectState> make(sen::kernel::RunApi& api,
                                                         sen::Object* instance,
                                                         sen::impl::WorkQueue* workQueue,
                                                         EventExplorer* eventExplorer,
                                                         PrinterRegistry& printerRegistry,
                                                         ReporterRegistry& reporterRegistry);
  ~ObjectState();

public:
  [[nodiscard]] bool& open();
  void window();
  void stopDrawing();

public:
  [[nodiscard]] sen::Object* getInstance() noexcept;
  [[nodiscard]] const sen::kernel::ProcessInfo* getOwnerInfo() const noexcept;

public:
  void startObservingElement(uint32_t elementHash, Plot* plot);
  void stopObservingElement(uint32_t elementHash, Plot* plot);

private:
  ObjectState(sen::kernel::RunApi& api,
              sen::Object* instance,
              sen::impl::WorkQueue* workQueue,
              EventExplorer* eventExplorer,
              PrinterRegistry& printerRegistry,
              ReporterRegistry& reporterRegistry);

private:
  void init();
  void close();
  void basicObjectInfo() const;
  void events();
  void properties();

private:
  struct ValuesGroup
  {
    std::string name;
    std::string label;
    std::vector<std::unique_ptr<PropertyData>> values;
    std::vector<MethodPrinter> methodPrinters;
  };

private:
  sen::kernel::RunApi& api_;
  std::shared_ptr<sen::Object> instance_;
  const sen::kernel::ProcessInfo* ownerInfo_ = nullptr;
  sen::impl::WorkQueue* workQueue_;
  EventExplorer* eventExplorer_;
  std::string windowLabel_;
  std::string elementLabel_;
  std::vector<ValuesGroup> valueGroups_;
  bool checked_ = false;
  ObservedDataMap observedData_;
  std::unordered_map<uint32_t, std::vector<Plot*>> observers_;
  std::vector<std::pair<std::shared_ptr<const sen::Event>, bool>> allEvents_;
  std::vector<sen::ConnectionGuard> connectionGuards_;
  PrinterRegistry& printerRegistry_;
  ReporterRegistry& reporterRegistry_;
  bool drawingStopped_ = false;
};

#endif  // SEN_COMPONENTS_EXPLORER_SRC_OBJECT_STATE_H
