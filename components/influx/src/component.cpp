// === component.cpp ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// component
#include "database.h"
#include "http.h"
#include "io_thread.h"
#include "recorder.h"
#include "tcp.h"
#include "udp.h"
#include "unix_datagram.h"
#include "unix_stream.h"
#include "util.h"

// sen
#include "sen/core/base/class_helpers.h"
#include "sen/core/base/compiler_macros.h"
#include "sen/core/base/duration.h"
#include "sen/core/base/result.h"
#include "sen/core/io/util.h"
#include "sen/kernel/component.h"
#include "sen/kernel/component_api.h"

// generated code
#include "stl/configuration.stl.h"
#include "stl/sen/kernel/basic_types.stl.h"

// asio
#include <asio/io_context.hpp>

// std
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

namespace sen::components::influx
{

class InfluxComponent: public kernel::Component
{
  SEN_NOCOPY_NOMOVE(InfluxComponent)

public:
  InfluxComponent() = default;
  ~InfluxComponent() override = default;

public:
  kernel::FuncResult run(kernel::RunApi& api) override
  {
    // read values from config
    auto config = toValue<Configuration>(api.getConfig());
    if (config.samplingPeriod.getNanoseconds() == 0)
    {
      constexpr auto defaultPeriod = Duration::fromHertz(60.0);
      config.samplingPeriod = defaultPeriod;
    }

    // warn if nothing to record
    if (config.selections.empty())
    {
      getLogger()->warn("no selections present; no data will be sent to Telegraf.");
      return done();
    }

    // create the transport named by the configuration. A scheme the host cannot provide throws
    // from its own constructor, and is reported below rather than ending the run.
    //
    // Declared above the transports, which hold a reference to it for the life of their sockets,
    // and above the IoThread near the end of this function, which runs it. Both of those orders
    // are what keeps the shutdown walk safe; io_thread.h says why.
    asio::io_context io;
    std::unique_ptr<Transport> transport;
    std::size_t maxMessageSize = 0U;
    // The protocol ceiling that applies to the chosen transport, or zero where none does. 65507 is
    // all an IPv4 datagram carries and 65527 an IPv6 one; a unix datagram is bounded by the socket
    // buffer rather than by a protocol, and an HTTP body is not a datagram at all.
    std::size_t messageCeiling = 0U;

    try
    {
      std::visit(
        ::sen::Overloaded {
          [&](const Tcp& value) { transport = std::make_unique<TCP>(io, value.address, value.port, value.family); },
          [&](const Udp& value)
          {
            messageCeiling = datagramCeilingFor(value.family);
            transport = std::make_unique<UDP>(io, value.address, value.port, value.family);
            maxMessageSize = value.maxMessageSize;
          },
#if defined(ASIO_HAS_LOCAL_SOCKETS)
          [&](const Unix& value) { transport = std::make_unique<UnixStream>(io, value.path); },
#else
          [&](const Unix&)
          {
            throwRuntimeError(
              "the Unix transport needs AF_UNIX stream sockets, which this platform does not "
              "provide; use Tcp, Udp or Http instead");
          },
#endif
          [&](const Http& value)
          {
            transport =
              std::make_unique<HttpTransport>(value.address, value.port, value.path, value.username, value.password);
            maxMessageSize = value.maxMessageSize;
          },
#if defined(ASIO_HAS_LOCAL_SOCKETS)
          [&](const UnixGram& value)
          {
            transport = std::make_unique<UnixDatagram>(io, value.path);
            maxMessageSize = value.maxMessageSize;
          }},
#else
          [&](const UnixGram&)
          {
            throwRuntimeError(
              "the UnixGram transport needs datagram AF_UNIX sockets, which this platform does "
              "not provide; use Tcp, Udp or Http instead");
          }},
#endif
        config.transport);
    }
    catch (const std::exception& err)
    {
      return Err(kernel::ExecError {kernel::ErrorCategory::expectationsNotMet, err.what()});
    }

    // set the compression, if any
    bool compressionRequested = false;
    try
    {
      std::visit(::sen::Overloaded {[&](const GZip& value)
                                    {
                                      transport->setCompressionEnabled(true, value.level);
                                      compressionRequested = true;
                                    },
                                    [](const NoCompression&) {}},
                 config.compression);
    }
    catch (const std::exception& err)
    {
      return Err(kernel::ExecError {kernel::ErrorCategory::expectationsNotMet, err.what()});
    }

    // Warn where the configuration cannot do what it says.
    if (compressionRequested && config.batchSize == 0U)
    {
      getLogger()->warn(
        "compression is on with no batching: every point becomes its own gzip "
        "stream, which is larger than the line it carries");
    }

    if (messageCeiling != 0U && maxMessageSize > messageCeiling)
    {
      getLogger()->warn("maxMessageSize of {} is above the {} bytes this transport's datagram can carry",
                        maxMessageSize,
                        messageCeiling);
    }

    transport->setMaxMessageSize(maxMessageSize);

    // create the database
    auto database = std::make_unique<Database>(std::move(transport));
    if (config.batchSize != 0)
    {
      database->batchOf(config.batchSize);
    }

    // The same number for both only where nothing compresses the batch; batchBoundFor says why.
    // Either way this flushes between lines, so it keeps a batch from growing past the bound rather
    // than guaranteeing anything: with batchSize 0 there is no batch to bound at all, and a single
    // line over the bound reaches the transport regardless, which drops it.
    database->messageOf(batchBoundFor(maxMessageSize, compressionRequested));

    // The sending moves to the IoThread near the end of this function. Writes before that thread
    // exists are not lost: a post to a context nobody is running yet simply waits, bounded by the
    // same queue. 10000 is what Telegraf's own agent defaults metric_buffer_limit to.
    constexpr std::size_t defaultQueueLimit = 10000U;
    database->sendThrough(io, config.queueLimit == 0U ? defaultQueueLimit : config.queueLimit);

    // create the recorder
    std::unique_ptr<Recorder> recorder;
    try
    {
      recorder = std::make_unique<Recorder>(std::move(database), config.selections.asVector(), api);
    }
    catch (const std::exception& err)
    {
      // A selection the component cannot read is a mistake in a configuration file. Letting it
      // throw reached std::terminate, so a typo ended the run with a crash report and read as a
      // fault in Sen rather than in the file.
      return Err(kernel::ExecError {kernel::ErrorCategory::expectationsNotMet, err.what()});
    }

    // Declared below the recorder, so its destructor runs first and joins before any socket on the
    // context is destroyed. Nothing is posted to it yet: the transports still do their I/O on the
    // thread that calls them.
    std::optional<IoThread> sending;
    try
    {
      sending.emplace(io);
    }
    catch (const std::exception& err)
    {
      // A thread the host will not give out would otherwise leave run() by throwing, and the
      // kernel logs that and rethrows it into std::terminate: the application being recorded would
      // die of a failure to record it.
      return Err(kernel::ExecError {kernel::ErrorCategory::runtimeError, err.what()});
    }

    getLogger()->info("sending to the telegraf listener");
    return api.execLoop(config.samplingPeriod, nullptr, false);
  }
};

}  // namespace sen::components::influx

SEN_COMPONENT(sen::components::influx::InfluxComponent)
