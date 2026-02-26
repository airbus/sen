// === source_info.h ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#ifndef SEN_KERNEL_SOURCE_INFO_H
#define SEN_KERNEL_SOURCE_INFO_H

#include "sen/core/obj/object_provider.h"
#include "sen/core/obj/object_source.h"

namespace sen::kernel
{

// forward declarations
class KernelApi;

namespace impl
{
class Session;
class KernelImpl;
}  // namespace impl

// Forward declarations
class SessionsDiscoverer;

/// Provides information about sources as strings. \ingroup kernel
class SourceInfo
{
public:
  SEN_NOCOPY_NOMOVE(SourceInfo)

public:
  virtual ~SourceInfo() = default;

public:
  /// The name of the session
  [[nodiscard]] const std::string& getName() const noexcept;

  /// Name of the buses discovered so far
  [[nodiscard]] std::vector<std::string> getDetectedSources() const;

  /// Sets a callback that will be invoked when sources are detected.
  /// The first time it will be called for each of the already-detected sources.
  /// Replaces any previously-set callback.
  void onSourceDetected(const std::function<void(const std::string&)>& callback);

  /// Sets a callback that will be invoked when sources are undetected.
  /// Replaces any previously-set callback.
  void onSourceUndetected(const std::function<void(const std::string&)>& callback);

protected:  // interface towards the owner
  friend class SessionsDiscoverer;
  friend class impl::Runner;

  explicit SourceInfo(std::string name);
  void addSource(const std::string& name);
  void removeSource(const std::string& name);
  virtual void drainInputs();
  void lock();
  void unlock();

private:
  void remove(const std::string& elem, std::vector<std::string>& vec) const;

private:
  using Mutex = std::recursive_mutex;
  using Lock = std::scoped_lock<Mutex>;

private:
  std::string name_;
  std::function<void(const std::string&)> detectedCallback_ = nullptr;
  std::function<void(const std::string&)> undetectedCallback_ = nullptr;

  mutable Mutex sourcesMutex_;
  std::vector<std::string> pendingAdditions_;
  std::vector<std::string> pendingRemovals_;
  std::vector<std::string> currentSources_;
};

/// \ingroup kernel
class SessionInfoProvider: public SourceInfo, public std::enable_shared_from_this<SessionInfoProvider>
{
public:
  SEN_NOCOPY_NOMOVE(SessionInfoProvider)

public:
  SessionInfoProvider(std::string name, std::shared_ptr<impl::Session> session);
  ~SessionInfoProvider() override;

private:
  friend class impl::Session;
  std::shared_ptr<impl::Session> session_;
};

class SessionsDiscoverer: public SourceInfo, public std::enable_shared_from_this<SessionsDiscoverer>
{
public:
  SEN_NOCOPY_NOMOVE(SessionsDiscoverer)

public:
  ~SessionsDiscoverer() override = default;

public:
  [[nodiscard]] std::shared_ptr<SessionInfoProvider> makeSessionInfoProvider(const std::string& sessionName);

protected:
  void drainInputs() override;

private:
  friend class KernelApi;
  friend class impl::KernelImpl;
  friend class impl::Runner;

  explicit SessionsDiscoverer(impl::Runner* owner);
  void sessionAvailable(const std::string& name);
  void sessionUnavailable(const std::string& name);

private:
  impl::Runner* owner_;
  std::vector<std::weak_ptr<SessionInfoProvider>> children_;
  std::vector<std::weak_ptr<SessionInfoProvider>> validChildren_;
};

}  // namespace sen::kernel

#endif  // SEN_KERNEL_SOURCE_INFO_H
