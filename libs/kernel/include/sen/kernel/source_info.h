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
  /// Returns the name of the session this source belongs to.
  /// @return Const reference to the session name string; valid for the lifetime of this object.
  [[nodiscard]] const std::string& getName() const noexcept;

  /// Returns the names of all buses (sources) discovered so far on this session.
  /// @return Vector of bus name strings at the time of the call.
  [[nodiscard]] std::vector<std::string> getDetectedSources() const;

  /// Sets a callback invoked whenever a new source (bus) is detected.
  /// On first registration, the callback is immediately invoked for each already-known source.
  /// Replaces any previously-set callback.
  /// @param callback  Function called with the bus name each time a new source appears.
  void onSourceDetected(const std::function<void(const std::string&)>& callback);

  /// Sets a callback invoked whenever a previously-known source (bus) disappears.
  /// Replaces any previously-set callback.
  /// @param callback  Function called with the bus name each time a source is lost.
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

/// Provides per-session source discovery â€” tracks which buses are live within a single session.
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

/// Discovers all sessions available on the network and exposes per-session `SessionInfoProvider` objects.
/// \ingroup kernel
class SessionsDiscoverer: public SourceInfo, public std::enable_shared_from_this<SessionsDiscoverer>
{
public:
  SEN_NOCOPY_NOMOVE(SessionsDiscoverer)

public:
  ~SessionsDiscoverer() override = default;

public:
  /// Returns (or creates) a `SessionInfoProvider` for the named session.
  /// @param sessionName  Name of the session to track.
  /// @return Shared pointer to the `SessionInfoProvider` for that session.
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
