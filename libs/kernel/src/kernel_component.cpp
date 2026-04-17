// === kernel_component.cpp ============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "./kernel_component.h"

// implementation
#include "kernel_impl.h"
#include "runner.h"

// sen
#include "sen/core/base/assert.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/timestamp.h"
#include "sen/core/meta/native_types.h"
#include "sen/core/meta/type.h"
#include "sen/core/meta/unit.h"
#include "sen/core/meta/unit_registry.h"
#include "sen/core/obj/object_source.h"
#include "sen/core/obj/subscription.h"
#include "sen/kernel/component_api.h"
#include "sen/kernel/detail/kernel_fwd.h"
#include "sen/kernel/kernel.h"

// generated code
#include "stl/sen/kernel/basic_types.stl.h"
#include "stl/sen/kernel/kernel_objects.stl.h"

// std
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sen::kernel::impl
{

namespace
{

//--------------------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------------------

constexpr auto kernelComponentPeriod = Duration::fromHertz(2.0f);

constexpr UnitCat toCat(UnitCategory category) noexcept
{
  switch (category)
  {
    case UnitCategory::length:
      return UnitCat::length;

    case UnitCategory::mass:
      return UnitCat::mass;

    case UnitCategory::time:
      return UnitCat::time;

    case UnitCategory::angle:
      return UnitCat::angle;

    case UnitCategory::temperature:
      return UnitCat::temperature;

    case UnitCategory::frequency:
      return UnitCat::frequency;

    case UnitCategory::velocity:
      return UnitCat::velocity;

    case UnitCategory::angularVelocity:
      return UnitCat::angularVelocity;

    case UnitCategory::acceleration:
      return UnitCat::acceleration;

    case UnitCategory::angularAcceleration:
      return UnitCat::angularAcceleration;

    case UnitCategory::density:
      return UnitCat::density;

    case UnitCategory::pressure:
      return UnitCat::pressure;

    case UnitCategory::area:
      return UnitCat::area;

    case UnitCategory::force:
      return UnitCat::force;

    case UnitCategory::torque:
      return UnitCat::torque;

    default:
      SEN_UNREACHABLE();
      return UnitCat::length;
  }
}

inline UnitInfo toInfo(const Unit& unit)
{
  UnitInfo info;
  info.name = unit.getName().data();
  info.abbreviation = unit.getAbbreviation().data();
  info.category = toCat(unit.getCategory());
  return info;
}

}  // namespace

//--------------------------------------------------------------------------------------------------------------
// VirtualKernelClock
//--------------------------------------------------------------------------------------------------------------

class VirtualKernelClock: public VirtualKernelClockBase<>
{
  SEN_NOCOPY_NOMOVE(VirtualKernelClock)

public:  // special members
  VirtualKernelClock(const std::string& name, KernelImpl* impl)
    : VirtualKernelClockBase<>(name)
    , impl_(impl)
    , virtualTimeMode_(impl_->getConfig().getParams().runMode == RunMode::virtualTime ||
                       impl_->getConfig().getParams().runMode == RunMode::virtualTimeRunning)
  {
  }

  ~VirtualKernelClock() override = default;

protected:
  void update(RunApi& runApi) override
  {
    if (!virtualTimeMode_)
    {
      setNextTime(runApi.getTime());
    }
  }

  [[nodiscard]] Duration processNoFlushImpl(Duration delta) override
  {
    auto result = impl_->advanceTimeDrainInputsAndUpdate(delta);
    setNextTime(getTime() + delta);
    return result;
  }

  [[nodiscard]] Duration flushOutputsImpl() override
  {
    commit(TimeStamp());
    impl_->flushVirtualTime();
    return impl_->computeNextExecutionDeltaTime();
  }

private:
  KernelImpl* impl_;
  bool virtualTimeMode_;
};

//--------------------------------------------------------------------------------------------------------------
// VirtualMasterClock
//--------------------------------------------------------------------------------------------------------------

class VirtualMasterClock: public VirtualMasterClockBase<>
{
  SEN_NOCOPY_NOMOVE(VirtualMasterClock)

public:  // special members
  explicit VirtualMasterClock(const std::string& name, RunApi& api, const std::string& source)
    : VirtualMasterClockBase<>(name), api_(api)
  {
    setNextDelta(Duration::fromHertz(30.0));
    virtualClocks_ = api.selectAllFrom<VirtualKernelClockInterface>(source);
  }

  ~VirtualMasterClock() override = default;

protected:
  Duration advanceTimeImpl(Duration duration) override
  {
    auto beforeExecution = std::chrono::high_resolution_clock::now();
    auto& clocks = virtualClocks_->list.getObjects();
    Duration elapsedVirtualTime {};

    if (clocks.empty())
    {
      elapsedVirtualTime = duration;
    }
    else
    {
      while (elapsedVirtualTime < duration)
      {
        singleStep(clocks);

        // if we did not advance anything, we are done
        if (stepDuration_.getNanoseconds() == 0)
        {
          elapsedVirtualTime = duration;
          break;
        }
        elapsedVirtualTime += stepDuration_;
      }
    }

    setNextTime(getTime() + elapsedVirtualTime);
    return {std::chrono::high_resolution_clock::now() - beforeExecution};
  }

  Duration stepImpl() override { return stepsImpl(1U); }

  Duration stepsImpl(uint64_t count) override
  {
    auto beforeExecution = std::chrono::high_resolution_clock::now();
    auto& clocks = virtualClocks_->list.getObjects();

    Duration elapsedVirtualTime {};
    if (!clocks.empty())
    {
      for (auto i = 0U; i < count; ++i)
      {
        singleStep(clocks);
        elapsedVirtualTime += stepDuration_;
      }
    }
    setNextTime(getTime() + elapsedVirtualTime);
    return {std::chrono::high_resolution_clock::now() - beforeExecution};
  }

private:
  void singleStep(const std::list<VirtualKernelClockInterface*>& clocks)
  {
    // call processNoFlush on all clocks
    forAllClocks(clocks,
                 [this](VirtualKernelClockInterface* clock, std::size_t& pending)
                 {
                   clock->processNoFlush(stepDuration_,
                                         {this,
                                          [&pending](const auto& result)
                                          {
                                            if (result.isError())
                                            {
                                              std::rethrow_exception(result.getError());
                                            }
                                            pending--;
                                          }});
                 });

    // call flushOutputs on all clocks and update the next step duration
    stepDuration_ = std::numeric_limits<Duration::ValueType>::max();
    forAllClocks(clocks,
                 [this](VirtualKernelClockInterface* clock, std::size_t& pending)
                 {
                   clock->flushOutputs({this,
                                        [this, &pending](const auto& result)
                                        {
                                          if (result.getValue() < stepDuration_)
                                          {
                                            stepDuration_ = result.getValue();
                                          }
                                          pending--;
                                        }});
                 });
  }

private:
  template <typename F>
  SEN_ALWAYS_INLINE void forAllClocks(const std::list<VirtualKernelClockInterface*>& clocks, F func) const
  {
    std::size_t pending = 0;

    for (auto* clock: clocks)
    {
      pending++;
      func(clock, pending);
    }

    auto* queue = api_.getWorkQueue();
    while (pending != 0)
    {
      queue->waitExecuteAll(Duration::fromHertz(1.0));
    }
  }

private:
  RunApi& api_;
  std::shared_ptr<Subscription<VirtualKernelClockInterface>> virtualClocks_;
  Duration stepDuration_;
};

//--------------------------------------------------------------------------------------------------------------
// KernelApiImpl
//--------------------------------------------------------------------------------------------------------------

class KernelApiImpl: public KernelApiBase
{
  SEN_NOCOPY_NOMOVE(KernelApiImpl)

public:  // special members
  KernelApiImpl(std::string name, KernelImpl* impl)
    : KernelApiBase(std::move(name), Kernel::getBuildInfo()), kernelImpl_(impl)
  {
    SEN_ASSERT(kernelImpl_);
    populateUnits();
    populateConfig();
  }

  ~KernelApiImpl() override = default;

protected:  // methods implementation
  void shutdownImpl() final { kernelImpl_->requestStop(0); }

  [[nodiscard]] UnitList getUnitsImpl() const final { return units_; }

  [[nodiscard]] StringList getTypesImpl() const final
  {
    populateTypes();
    return types_;
  }

  [[nodiscard]] KernelParams getConfigImpl() const final { return config_; }

private:
  void populateUnits()
  {
    for (auto elem: UnitRegistry::get().getUnits())
    {
      units_.push_back(toInfo(*elem));
    }
  }

  void populateConfig() { config_ = kernelImpl_->getConfig().getParams(); }

  void populateTypes() const
  {
    types_.clear();

    const auto& native = getNativeTypes();
    auto custom = kernelImpl_->getTypes().getAll();
    types_.reserve(native.size() + custom.size());

    // native
    for (const auto& elem: native)
    {
      types_.emplace_back(elem->getName());
    }

    // custom
    for (const auto& [name, type]: custom)
    {
      types_.push_back(name);
    }
  }

private:
  KernelImpl* kernelImpl_;
  UnitList units_;
  mutable StringList types_;
  KernelParams config_;
  std::vector<std::shared_ptr<Connection>> connections_;
};

//--------------------------------------------------------------------------------------------------------------
// KernelComponent
//--------------------------------------------------------------------------------------------------------------

KernelComponent::KernelComponent(KernelImpl* impl): impl_(impl) { SEN_ASSERT(impl_ != nullptr); }

FuncResult KernelComponent::run(RunApi& api)
{
  const auto& params = impl_->getConfig().getParams();
  bool needsVirtualTime = params.runMode == RunMode::virtualTime || params.runMode == RunMode::virtualTimeRunning;

  // get the sources
  auto objectsBus = api.getSource(params.bus);
  std::shared_ptr<ObjectSource> clockBus;

  // add the api
  auto kernelApiObject = std::make_shared<KernelApiImpl>("api", impl_);
  objectsBus->add(kernelApiObject);

  // add the virtual clock, if needed
  std::shared_ptr<VirtualKernelClock> kernelClock;
  std::shared_ptr<VirtualMasterClock> masterClock;
  if (needsVirtualTime)
  {
    clockBus = objectsBus;

    // open a different clock bus, if needed
    if (params.clockBus != params.bus)
    {
      clockBus = api.getSource(params.clockBus);
    }

    kernelClock = std::make_shared<VirtualKernelClock>(params.clockName, impl_);
    clockBus->add(kernelClock);

    if (params.clockMaster)
    {
      masterClock = std::make_shared<VirtualMasterClock>("master", api, params.clockBus);
      clockBus->add(masterClock);
    }
  }

  FuncResult result = done();
  if (needsVirtualTime)
  {
    // find our runner
    Runner* ourRunner = nullptr;
    for (auto& runner: impl_->getRunners())
    {
      if (runner->getComponentContext().instance == this)
      {
        ourRunner = runner.get();
        break;
      }
    }

    SEN_ASSERT(ourRunner != nullptr);

    // make our runner go as fast as possible
    while (!api.stopRequested())
    {
      ourRunner->drainUntilEventOrTimeout(Duration::fromHertz(90.0f));
      ourRunner->update();
      ourRunner->commit();
    }
  }
  else
  {
    result = api.execLoop(kernelComponentPeriod, []() {}, false);
  }

  // remove objects
  objectsBus->remove(kernelApiObject);

  if (needsVirtualTime)
  {
    if (kernelClock)
    {
      clockBus->remove(kernelClock);
    }

    if (masterClock)
    {
      clockBus->remove(masterClock);
    }
  }

  return result;
}

bool KernelComponent::isRealTimeOnly() const noexcept { return true; }

}  // namespace sen::kernel::impl
