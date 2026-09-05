// === network_exclusion.cpp ===========================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

#include "network_exclusion.h"

#include "util.h"

// sen
#include "sen/core/base/checked_conversions.h"
#include "sen/core/base/result.h"

// asio
#include <asio/error_code.hpp>
#include <asio/ip/address_v4.hpp>

// generated code
#include "stl/configuration.stl.h"

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef _WIN32_DCOM
#    define _WIN32_DCOM
#  endif
#  include <Wbemidl.h>
#  include <comdef.h>
#endif

// std
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace sen::components::ether
{

namespace
{

constexpr uint16_t lastWellKnownPort = 1023;
constexpr uint16_t lastPort = std::numeric_limits<uint16_t>::max();

// RFC 2365 reserves these addresses in each scoped zone
constexpr std::string_view organizationLocalReservedMin = "239.195.255.0";
constexpr std::string_view organizationLocalReservedMax = "239.195.255.255";
constexpr std::string_view localScopeReservedMin = "239.255.255.0";
constexpr std::string_view localScopeReservedMax = "239.255.255.255";

[[nodiscard]] Result<asio::ip::address_v4, std::string> parseAddress(std::string_view address)
{
  asio::error_code error;
  const auto result = asio::ip::make_address_v4(std::string(address), error);
  if (error)
  {
    return Err(std::string("invalid multicast exclusion address '") + std::string(address) + "'");
  }
  return Ok(result);
}

[[nodiscard]] Result<void, std::string> addMulticastRange(MulticastExclusions& exclusions,
                                                          std::string_view min,
                                                          std::string_view max)
{
  const auto minResult = parseAddress(min);
  if (minResult.isError())
  {
    return Err(minResult.getError());
  }
  const auto maxResult = parseAddress(max);
  if (maxResult.isError())
  {
    return Err(maxResult.getError());
  }

  const auto& minAddress = minResult.getValue();
  const auto& maxAddress = maxResult.getValue();
  if (minAddress.to_uint() > maxAddress.to_uint())
  {
    return Err(std::string("invalid multicast exclusion range (min > max)"));
  }
  if ((minAddress.to_uint() >> 24U) != 239U || (maxAddress.to_uint() >> 24U) != 239U)
  {
    return Err(std::string("multicast exclusion ranges must stay inside 239.0.0.0/8"));
  }
  exclusions.add(minAddress.to_uint(), maxAddress.to_uint());
  return Ok();
}

[[nodiscard]] uint64_t multicastSpaceSize(const MulticastRange& range)
{
  uint64_t result = 1;
  for (const auto& byteRange: range)
  {
    const auto min = sen::std_util::checkedConversion<uint64_t>(byteRange.min);
    const auto max = sen::std_util::checkedConversion<uint64_t>(byteRange.max);
    result *= max - min + 1U;
  }
  return result;
}

[[nodiscard]] uint32_t nextMulticastAddress(uint32_t currentAddress, const MulticastRange& range)
{
  auto bytes = asio::ip::make_address_v4(currentAddress).to_bytes();
  for (int index = static_cast<int>(bytes.size()) - 1; index >= 0; --index)
  {
    auto& byte = bytes.at(index);
    const auto& byteRange = range.at(index);

    if (byte < byteRange.max)
    {
      ++byte;
      return asio::ip::address_v4(bytes).to_uint();
    }
    byte = byteRange.min;
  }
  return asio::ip::address_v4(bytes).to_uint();
}

#ifdef __linux__
[[nodiscard]] Result<uint16_t, std::string> parsePort(std::string_view text)
{
  std::istringstream input {std::string(text)};
  uint32_t value = 0;
  input >> value;
  if (!input || !input.eof() || value > lastPort)
  {
    return Err(std::string("invalid port '") + std::string(text) + "'");
  }

  return Ok(sen::std_util::checkedConversion<uint16_t>(value));
}

[[nodiscard]] Result<void, std::string> parsePortRanges(std::string_view text, OsPortExclusions& exclusions)
{
  std::size_t begin = 0;
  while (begin < text.size())
  {
    const auto end = text.find(',', begin);
    const auto token = text.substr(begin, end == std::string_view::npos ? text.size() - begin : end - begin);
    if (token.empty())
    {
      return Err(std::string("empty port range"));
    }

    const auto separator = token.find('-');
    const auto minResult = parsePort(token.substr(0, separator));
    if (minResult.isError())
    {
      return Err(minResult.getError());
    }

    const auto maxResult = separator == std::string_view::npos ? minResult : parsePort(token.substr(separator + 1));
    if (maxResult.isError())
    {
      return Err(maxResult.getError());
    }
    if (minResult.getValue() > maxResult.getValue())
    {
      return Err(std::string("invalid port exclusion range (min > max)"));
    }

    exclusions.add(minResult.getValue(), maxResult.getValue());
    if (end == std::string_view::npos)
    {
      break;
    }
    begin = end + 1;
  }
  return Ok();
}

#endif

#ifdef _WIN32
/// COM interface pointer used for the Windows WMI port range queries
template <typename Interface>
class WmiComInterfacePtr
{
public:
  WmiComInterfacePtr() = default;
  ~WmiComInterfacePtr() { reset(nullptr); }

  WmiComInterfacePtr(const WmiComInterfacePtr&) = delete;
  WmiComInterfacePtr& operator=(const WmiComInterfacePtr&) = delete;

  Interface* get() const noexcept { return ptr_; }
  Interface* operator->() const noexcept { return ptr_; }

  void reset(Interface* ptr) noexcept
  {
    if (ptr_ != nullptr)
    {
      ptr_->Release();
    }
    ptr_ = ptr;
  }

private:
  Interface* ptr_ = nullptr;
};

/// COM initialization state for the Windows WMI port range queries
class WmiComInitialization
{
public:
  explicit WmiComInitialization(bool uninitialize) noexcept: uninitialize_(uninitialize) {}
  ~WmiComInitialization()
  {
    if (uninitialize_)
    {
      CoUninitialize();
    }
  }

  WmiComInitialization(const WmiComInitialization&) = delete;
  WmiComInitialization& operator=(const WmiComInitialization&) = delete;
  WmiComInitialization(WmiComInitialization&& other) noexcept: uninitialize_(std::exchange(other.uninitialize_, false))
  {
  }
  WmiComInitialization& operator=(WmiComInitialization&& other) noexcept
  {
    if (this != &other)
    {
      if (uninitialize_)
      {
        CoUninitialize();
      }
      uninitialize_ = std::exchange(other.uninitialize_, false);
    }
    return *this;
  }

private:
  bool uninitialize_ = false;
};

/// WMI property value returned through the COM VARIANT type
class WmiVariantValue
{
public:
  WmiVariantValue() { VariantInit(&value_); }
  ~WmiVariantValue() { VariantClear(&value_); }

  WmiVariantValue(const WmiVariantValue&) = delete;
  WmiVariantValue& operator=(const WmiVariantValue&) = delete;

  VARIANT* put() noexcept { return &value_; }
  const VARIANT& get() const noexcept { return value_; }

private:
  VARIANT value_ {};
};

[[nodiscard]] std::string wmiNameToString(std::wstring_view text) { return std::string(text.begin(), text.end()); }

[[nodiscard]] std::string hresultToString(HRESULT result)
{
  std::ostringstream output;
  output << "0x" << std::hex << static_cast<unsigned long>(result);
  return output.str();
}

[[nodiscard]] std::string variantTypeToString(VARTYPE type)
{
  std::ostringstream output;
  output << static_cast<uint32_t>(type) << " (0x" << std::hex << static_cast<uint32_t>(type) << ")";
  return output.str();
}

[[nodiscard]] Result<WmiComInitialization, std::string> initializeWmiCom()
{
  // WMI queries require COM to be initialized in the current thread
  const auto initResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  const bool shouldUninitialize = SUCCEEDED(initResult);
  if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE)
  {
    return Err(std::string("could not initialize COM: ") + hresultToString(initResult));
  }

  const auto securityResult = CoInitializeSecurity(
    nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);
  if (FAILED(securityResult) && securityResult != RPC_E_TOO_LATE)
  {
    if (shouldUninitialize)
    {
      CoUninitialize();
    }
    return Err(std::string("could not initialize COM security: ") + hresultToString(securityResult));
  }

  return Ok(WmiComInitialization(shouldUninitialize));
}

[[nodiscard]] Result<std::optional<uint32_t>, std::string> wmiValueToOptionalUint32(VARIANT* value)
{
  if (value == nullptr)
  {
    return Err(std::string("null WMI value"));
  }
  if (value->vt == VT_EMPTY || value->vt == VT_NULL)
  {
    return Ok(std::optional<uint32_t> {});
  }

  WmiVariantValue converted;
  const auto convertResult = VariantChangeType(converted.put(), value, 0, VT_UI4);
  if (FAILED(convertResult))
  {
    return Err(std::string("could not convert WMI value type ") + variantTypeToString(value->vt) + ": " +
               hresultToString(convertResult));
  }

  return Ok(std::optional<uint32_t> {converted.get().ulVal});
}

[[nodiscard]] Result<std::optional<uint32_t>, std::string> readOptionalWmiUint32Property(IWbemClassObject* object,
                                                                                         const wchar_t* propertyName,
                                                                                         std::wstring_view className)
{
  WmiVariantValue value;
  const auto result = object->Get(propertyName, 0, value.put(), nullptr, nullptr);
  if (FAILED(result))
  {
    return Err(std::string("could not read WMI property '") + wmiNameToString(propertyName) + "' from " +
               wmiNameToString(className) + ": " + hresultToString(result));
  }

  auto numericResult = wmiValueToOptionalUint32(value.put());
  if (numericResult.isError())
  {
    return Err(std::string("invalid WMI property '") + wmiNameToString(propertyName) + "' from " +
               wmiNameToString(className) + ": " + numericResult.getError());
  }

  return numericResult;
}

[[nodiscard]] Result<void, std::string> addDynamicPortRangeFromWmiClass(IWbemServices* service,
                                                                        std::wstring_view className,
                                                                        OsPortExclusions& exclusions,
                                                                        bool allowMissingClass)
{
  std::wstring query = L"SELECT DynamicPortRangeStartPort, DynamicPortRangeNumberOfPorts FROM ";
  query.append(className.data(), className.size());
  IEnumWbemClassObject* enumeratorRaw = nullptr;
  const auto queryResult = service->ExecQuery(_bstr_t(L"WQL"),
                                              _bstr_t(query.c_str()),
                                              WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                                              nullptr,
                                              &enumeratorRaw);
  if (FAILED(queryResult))
  {
    if (allowMissingClass && queryResult == WBEM_E_INVALID_CLASS)
    {
      return Ok();
    }
    return Err(std::string("could not query WMI class ") + wmiNameToString(className) + ": " +
               hresultToString(queryResult));
  }
  WmiComInterfacePtr<IEnumWbemClassObject> enumerator;
  enumerator.reset(enumeratorRaw);

  bool foundRange = false;
  while (true)
  {
    IWbemClassObject* objectRaw = nullptr;
    ULONG objectCount = 0;
    const auto nextResult = enumerator->Next(WBEM_INFINITE, 1, &objectRaw, &objectCount);
    if (FAILED(nextResult))
    {
      return Err(std::string("could not read WMI query result from ") + wmiNameToString(className) + ": " +
                 hresultToString(nextResult));
    }
    if (objectCount == 0)
    {
      break;
    }
    WmiComInterfacePtr<IWbemClassObject> object;
    object.reset(objectRaw);

    const auto startResult = readOptionalWmiUint32Property(object.get(), L"DynamicPortRangeStartPort", className);
    if (startResult.isError())
    {
      return Err(startResult.getError());
    }
    const auto countResult = readOptionalWmiUint32Property(object.get(), L"DynamicPortRangeNumberOfPorts", className);
    if (countResult.isError())
    {
      return Err(countResult.getError());
    }

    const auto& start = startResult.getValue();
    const auto& count = countResult.getValue();
    if (!start && !count)
    {
      continue;
    }
    if (!start || !count)
    {
      return Err(std::string("Windows dynamic port range is incomplete in ") + wmiNameToString(className));
    }

    if (*count == 0U)
    {
      continue;
    }

    if (*start == 0U || *start > lastPort || *count > lastPort || *count > (lastPort - *start + 1U))
    {
      return Err(std::string("invalid Windows dynamic port range reported by ") + wmiNameToString(className));
    }

    const auto max = *start + *count - 1U;
    exclusions.add(sen::std_util::checkedConversion<uint16_t>(*start), sen::std_util::checkedConversion<uint16_t>(max));
    foundRange = true;
  }

  if (!foundRange)
  {
    if (allowMissingClass)
    {
      return Ok();
    }
    return Err(std::string("WMI class ") + wmiNameToString(className) + " did not report a dynamic port range");
  }

  return Ok();
}

#endif

[[nodiscard]] Result<OsPortExclusions, std::string> probeOsPortExclusions()
{
  OsPortExclusions result;
#ifdef __linux__
  const std::filesystem::path ephemeralPortsPath = "/proc/sys/net/ipv4/ip_local_port_range";
  const std::filesystem::path reservedPortsPath = "/proc/sys/net/ipv4/ip_local_reserved_ports";

  std::ifstream ephemeralInput(ephemeralPortsPath);
  if (!ephemeralInput)
  {
    return Err(std::string("could not read OS ephemeral port range from '") + ephemeralPortsPath.string() + "'");
  }

  uint32_t ephemeralMin = 0;
  uint32_t ephemeralMax = 0;
  ephemeralInput >> ephemeralMin;
  if (!ephemeralInput)
  {
    return Err(std::string("invalid OS ephemeral port range in '") + ephemeralPortsPath.string() + "'");
  }
  ephemeralInput >> ephemeralMax;
  if (!ephemeralInput || ephemeralMin > lastPort || ephemeralMax > lastPort || ephemeralMin > ephemeralMax)
  {
    return Err(std::string("invalid OS ephemeral port range in '") + ephemeralPortsPath.string() + "'");
  }

  result.add(sen::std_util::checkedConversion<uint16_t>(ephemeralMin),
             sen::std_util::checkedConversion<uint16_t>(ephemeralMax));

  std::ifstream reservedInput(reservedPortsPath);
  if (!reservedInput)
  {
    return Err(std::string("could not read OS reserved port ranges from '") + reservedPortsPath.string() + "'");
  }

  std::string configuredRanges;
  std::getline(reservedInput, configuredRanges);
  if (!reservedInput && !reservedInput.eof())
  {
    return Err(std::string("could not read OS reserved port ranges from '") + reservedPortsPath.string() + "'");
  }

  const auto parseResult = parsePortRanges(configuredRanges, result);
  if (parseResult.isError())
  {
    return Err(std::string("invalid OS reserved port ranges in '") + reservedPortsPath.string() +
               "': " + parseResult.getError());
  }
#endif
#ifdef _WIN32
  auto comResult = initializeWmiCom();
  if (comResult.isError())
  {
    return Err(comResult.getError());
  }
  auto WmiComInitialization = std::move(comResult).getValue();

  IWbemLocator* locatorRaw = nullptr;
  const auto locatorResult = CoCreateInstance(
    CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locatorRaw));
  if (FAILED(locatorResult))
  {
    return Err(std::string("could not create WMI locator: ") + hresultToString(locatorResult));
  }
  WmiComInterfacePtr<IWbemLocator> locator;
  locator.reset(locatorRaw);

  // MSFT_Net*Setting classes are exposed through ROOT\StandardCimv2
  IWbemServices* serviceRaw = nullptr;
  const auto connectResult = locator->ConnectServer(
    _bstr_t(L"ROOT\\StandardCimv2"), nullptr, nullptr, nullptr, 0, nullptr, nullptr, &serviceRaw);
  if (FAILED(connectResult))
  {
    return Err(std::string("could not connect to WMI namespace ROOT\\StandardCimv2: ") +
               hresultToString(connectResult));
  }
  WmiComInterfacePtr<IWbemServices> service;
  service.reset(serviceRaw);

  const auto proxyResult = CoSetProxyBlanket(service.get(),
                                             RPC_C_AUTHN_WINNT,
                                             RPC_C_AUTHZ_NONE,
                                             nullptr,
                                             RPC_C_AUTHN_LEVEL_CALL,
                                             RPC_C_IMP_LEVEL_IMPERSONATE,
                                             nullptr,
                                             EOAC_NONE);
  if (FAILED(proxyResult))
  {
    return Err(std::string("could not configure WMI proxy security: ") + hresultToString(proxyResult));
  }

  // read the Windows dynamic port ranges
  auto tcpResult = addDynamicPortRangeFromWmiClass(service.get(), L"MSFT_NetTCPSetting", result, false);
  if (tcpResult.isError())
  {
    return Err(std::string("could not read Windows TCP dynamic port range: ") + tcpResult.getError());
  }
  auto udpResult = addDynamicPortRangeFromWmiClass(service.get(), L"MSFT_NetUDPSetting", result, true);
  if (udpResult.isError())
  {
    return Err(std::string("could not read Windows UDP dynamic port range: ") + udpResult.getError());
  }
#endif
  return Ok(std::move(result));
}

}  // namespace

Result<NetworkExclusions, std::string> makeNetworkExclusions(const Configuration& config)
{
  NetworkExclusions result;

  if (auto addResult = addMulticastRange(result.multicast, organizationLocalReservedMin, organizationLocalReservedMax);
      addResult.isError())
  {
    return Err(addResult.getError());
  }
  if (auto addResult = addMulticastRange(result.multicast, localScopeReservedMin, localScopeReservedMax);
      addResult.isError())
  {
    return Err(addResult.getError());
  }

  for (const auto& configuredRange: config.busConfig.multicastExclusions)
  {
    const auto addResult = addMulticastRange(result.multicast, configuredRange.min, configuredRange.max);
    if (addResult.isError())
    {
      return Err(addResult.getError());
    }
  }

  result.ports.builtIn.add(0, lastWellKnownPort);
  for (const auto& range: config.portExclusions)
  {
    if (range.min > range.max)
    {
      return Err(std::string("invalid port exclusion range (min > max)"));
    }
    result.ports.configured.add(range.min, range.max);
  }

  // A failed probe is not a reason to refuse to start. It fails where the OS will not say which
  // ports it reserves -- a hardened container without /proc/sys/net/ipv4/ip_local_port_range, or
  // Windows without WMI -- and the set it produces only narrows the choice of pinned ports, which
  // are opt-in. Starting with an empty set risks a collision the OS would have warned about;
  // refusing to start denies the whole component over an advisory list.
  auto osResult = probeOsPortExclusions();
  if (osResult.isError())
  {
    getLogger()->warn(
      "could not read the OS port exclusions ({}); continuing without them, so a "
      "pinned port may collide with one the OS has reserved",
      osResult.getError());
  }
  else
  {
    result.ports.os = std::move(osResult).getValue();
  }

  return Ok(std::move(result));
}

bool hasUsableMulticastAddress(const MulticastRange& range, const MulticastExclusions& exclusions)
{
  asio::ip::address_v4::bytes_type bytes {};
  for (std::size_t index = 0; index < bytes.size(); ++index)
  {
    bytes.at(index) = range.at(index).min;
  }

  return getUsableMulticastAddress(asio::ip::address_v4(bytes), range, exclusions).has_value();
}

[[nodiscard]] std::optional<asio::ip::address_v4> getUsableMulticastAddress(asio::ip::address_v4 candidate,
                                                                            const MulticastRange& range,
                                                                            const MulticastExclusions& exclusions)
{
  const auto address =
    exclusions.nextUsable(candidate.to_uint(),
                          multicastSpaceSize(range),
                          [&range](uint32_t address) { return nextMulticastAddress(address, range); });

  if (!address)
  {
    return std::nullopt;
  }

  return asio::ip::make_address_v4(*address);
}

}  // namespace sen::components::ether
