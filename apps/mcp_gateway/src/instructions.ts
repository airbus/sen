// === instructions.ts =================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// `instructions` shipped on MCP `initialize`. Concept primer the LLM reads once per connect.
// Tool descriptions cover the per-call contracts and don't repeat any concepts taught here.

import { EVENT_BUFFER_BYTE_CAP, EVENT_BUFFER_CAP } from "./kernel.js";

export const INSTRUCTIONS = `Sen is an object-oriented distributed system. A running Sen kernel exposes a live graph of
objects; this gateway lets you observe and interact with one or more kernels.

KERNELS
  A Sen kernel is a running Sen process. The gateway can be connected to zero or many at
  once. Tools that touch live data take an optional \`kernel\` argument naming which one to
  target; with exactly one kernel connected the argument can be omitted.

  Lifecycle tools: connectToKernel, disconnectFromKernel, listKernels. Each tool's
  description carries its argument shape; this primer covers only the conceptual model.

  connectToKernel needs both a name and a URL. The user usually volunteers one. "Connect
  to staging" gives you a name and a need to ask for the URL; "connect to ws://..." gives
  you a URL and a need to ask what to call it. For the FIRST kernel you may pick a name
  yourself without asking. For subsequent kernels, use the name the user implied; if
  context doesn't make one obvious, ask: "What would you like to call this one?" Once
  registered, the gateway-side name is fixed; if the user later refers to it by a different
  label, keep your own mapping. If unsure which kernel the user means, call listKernels first.

NAMESPACE
  Objects live in a three-level hierarchy: <session>.<bus>.<objectName>.
  Sessions group unrelated systems; buses partition a session's object graph.
  An object's full name is qualified by both (e.g. monitoring.headquarters.sensor42).

OBJECTS
  Each object is an instance of a CLASS defined in STL (Sen Type Language) or HLA. A class has:
    properties (typed fields), in four categories:
      staticRO  : set at construction, never changes
      staticRW  : set at construction, exposed setter (rare)
      dynamicRO : changes over time; only the owning component writes
      dynamicRW : changes over time; setter exposed
    methods (callable functions), optionally marked \`constant\` (no state change) or
      \`deferred\` (the callee can batch; response may take longer than usual)
    events (emitted notifications), fired asynchronously when the owning object decides
  When you call \`getType\` on a class you get a CustomTypeSpec describing all three.

TYPE SYSTEM (what shows up inside a CustomTypeSpec)
  Sen types: integers (u8, u16, u32, u64, i16, i32, i64; there is no i8, use u8 for an 8-bit
  integer), floats (f32, f64), bool, string, TimeStamp, Duration, enums, structs (with
  inheritance), variants (type-safe unions), sequences (bounded, unbounded, fixed), quantities
  (numeric value + unit), optionals, and class references.

  64-bit integers cross the JSON hop as decimal strings (\`"12345"\`) to preserve precision above
  2^53. Reads (\`getProperty\`, \`getObjectsState\`, event payloads, method return values) yield
  strings for \`i64\` / \`u64\` fields; writes (\`setProperty\`, \`invokeMethod\` args) accept either
  a JSON number or a JSON string. All other integer widths and all floats remain plain JSON numbers.

EXECUTION MODEL
  Sen components run in discrete cycles. Implication for clients: when you write a property
  or invoke a method, the change is not observable until the owning component's next cycle
  (typically tens of milliseconds). A \`getProperty\` immediately after a \`setProperty\` may
  still return the old value, so poll briefly if you need to confirm an effect landed.
  Similarly, a method call may complete before its side effects are visible elsewhere.

QUALITY OF SERVICE
  Each property, method and event carries a transportMode, and getType reports it as one of
  \`confirmed\` (TCP, reliable and ordered), \`unicast\` or \`multicast\` (both UDP, fast, may
  drop). Those are the only three values you will see; \`bestEffort\` is how the STL author
  writes it and never appears in a spec. The default differs by kind: methods are confirmed
  unless the STL opted out, so a call you make is reliable; properties and events are not. For property updates the
  choice is usually invisible to consumers. For events it decides whether a drop is possible,
  so check the spec before you rely on one arriving.

INTERESTS
  To observe (or interact with) any object you first declare an INTEREST: a standing live query
  over a bus. The interest stays open until you release it; its match-set updates live as
  objects appear and disappear. Every per-object operation (\`getProperty\`, \`setProperty\`,
  \`invokeMethod\`, event subscriptions) takes the interest name as a handle. Always
  \`releaseInterest\` when done; otherwise the server keeps streaming updates for nothing.

  If you plan to write or invoke, declare the interest with \`withSchemas: true\` so every type
  involved in matched objects ships with its JSON-Schema. You can then shape values and
  arguments correctly without an extra \`getType\` round-trip per type.

OPERATING PRINCIPLES

  1. Discover before you assume. Class names, bus names, and property shapes are
     project-specific; don't guess them from the user's prompt or your training data.
     Call \`listSessions\` for bus names and \`getTypes\` / \`getType\` for class names BEFORE
     you reference them in a query or a write. One discovery round-trip is cheaper than a
     failed \`declareInterest\` you have to retry.

  2. Snapshot, don't loop. When you want the full state of one or more objects, use
     \`getObjectsState\` in a single call instead of looping \`getProperty\`. See READING STATE.

  3. Reuse open interests. Before declaring a new one, check \`listInterests\`. If an open
     interest's query already covers what the user is asking (or a superset of it), reuse it.
     Interests are session-scoped working sets, not one-shot queries. The gateway caps each
     kernel at 64 open interests; an LLM that opens a fresh one per question will hit that
     limit and start failing. Always \`releaseInterest\` when done.

  4. Filter the match-set yourself when you can. \`listObjects\` and \`getObjectsState\` return
     every matched object's className. If you only care about one class out of a broader
     interest, just keep the entries whose \`className\` matches; no narrower interest needed.
     Reach for a narrower SQL query only when the filter depends on property values
     (\`WHERE ...\`) or when the broader match-set is too large to list comfortably.

QUERY SYNTAX (Sen Query Language)
  Grammar:
    SELECT <className> FROM <session>.<bus> [WHERE <expr>]
    SELECT * FROM <session>.<bus>                    : match every object in the bus
  The FROM clause ALWAYS uses the qualified <session>.<bus> form. Buses do not exist on
  their own; a bare bus name (e.g. \`FROM tutorial\`) is a syntax error. \`listSessions\`
  returns sessions and their bus names separately; build \`<session>.<bus>\` from those.
  Operators: =, >, <, >=, <=, BETWEEN x AND y, IN (...), combined with AND / OR / NOT.
  Identifiers: property names, the pseudo-properties \`name\` (string) and \`id\` (uint),
    struct-field paths (\`position.x\`), variant-qualified paths (\`spatial.SpatialFPStruct.lon\`).
  Values: "double-quoted" strings, true / false, integers, floats with a dot,
    enum literals quoted but unqualified (e.g. "friendly").
  Examples:
    SELECT school.Student FROM school.primary
    SELECT school.Student FROM school.primary WHERE focusLevel BETWEEN 0.0 AND 0.5
    SELECT rpr.Aircraft FROM se.env WHERE forceIdentifier IN ("friendly", "neutral")
                                     AND spatial.SpatialFPStruct.lat >= 34.0

READING STATE
  \`getProperty\` returns one value; \`getObjectsState\` returns every property of one or many
  matched objects in a single call (walks the inheritance chain). Snapshot rather than loop:
  one batched \`getObjectsState\` is much cheaper than N \`getProperty\` calls. Returned values
  are parsed Vars; primitives as JS scalars, structs as nested objects, sequences as arrays.

WRITING STATE
  \`setProperty\` writes; only \`staticRW\` / \`dynamicRW\` categories accept writes. Value shape
  follows the property's JSON-Schema, which is bundled with the interest match-set when the
  interest was declared with \`withSchemas: true\`. The owning component may overwrite on its
  next cycle, so re-read with \`getProperty\` if you need to confirm what stuck.

INVOKING METHODS
  \`invokeMethod\` calls a method and returns its parsed return value (null for void methods).
  The gateway awaits the async Sen response so the call is synchronous-looking from your side;
  for methods declared \`deferred\` the response may be slower than usual. Methods marked
  \`constant\` don't change object state; non-constant methods may (and usually do).

EVENTS
  Events are async-fire from the owning object; MCP doesn't carry long-running streams, so the
  gateway buffers events server-side. \`subscribeEvent\` starts buffering one event on one
  object; \`pollEvents\` drains the per-interest buffer; \`unsubscribeEvent\` cancels one
  subscription; \`unsubscribeAllEvents\` cancels every subscription open under an interest.
  All events subscribed under the same interest share a single FIFO, capped at ${EVENT_BUFFER_CAP} entries
  or ${EVENT_BUFFER_BYTE_CAP / (1024 * 1024)} MiB, whichever it reaches first, dropping the oldest to make
  room. A single event bigger than the whole budget is still kept, so a poll never comes back
  empty just because one payload was large. Poll regularly when tracking a chatty event, or subscribe rare
  events under their own interest so a chatty event can't starve them. Releasing an interest
  drains the buffer and returns the residual entries.

RECORDING ANALYSIS
  Independent of the live kernel tools above. Three tools cover offline analysis via the
  \`sen_db_python\` bindings: \`listRecordings\` enumerates recordings under a caller-supplied
  root, \`runRecordingScript\` spawns python3 against your code and returns its output, and
  \`getRecordingDocs\` returns the full bindings reference. The user must supply the recordings
  root path; ask for it if they haven't. Scripts are self-contained:
  \`import sen_db_python as sen\`, \`sen.Input("/path/...")\`, walk a cursor, aggregate. Fetch
  \`getRecordingDocs\` (or read the \`sen://docs/users-guide/db-python-bindings\` resource)
  before authoring a script; the bindings have a specific shape (cursor.advance(),
  isinstance dispatch on entry.payload, snapshot reflection, type-registry cross-reference)
  and the reference is the source of truth. stdout and stderr are capped server-side, so
  aggregate inside the script and don't dump raw rows.

TYPICAL FLOW
   1. connectToKernel({name, url})                   : register a kernel (skip if listKernels shows one)
   2. listSessions                                   : discover sessions and their buses
   3. getTypes / getType                             : explore class shapes (withSchema:true for portable JSON-Schema)
   4. declareInterest (withSchemas:true if writing)  : open a standing query on a bus
   5. listObjects / getObjectsState                  : enumerate or snapshot the match-set
   6. getProperty / setProperty                      : read or write one property
   7. invokeMethod                                   : call a method
   8. subscribeEvent + pollEvents + unsubscribeEvent : observe emitted events
   9. releaseInterest                                : close when done
  10. disconnectFromKernel({name})                   : tear down (optional; gateway shutdown does this too)

WORKED EXAMPLE (illustrative; the shape your kernel returns will reflect its own STL)
  -> connectToKernel({ name: "local", url: "ws://127.0.0.1:8080" })
  <- { "connected": "local" }
  -> listSessions
  <- [{ "name": "school", "buses": ["primary", "secondary"] }]
  -> declareInterest({ name: "primaryStudents",
                       query: "SELECT school.Student FROM school.primary",
                       withSchemas: true })
  <- { "kernel": "local", "name": "primaryStudents",
       "matched": [{ "name": "alice", "className": "school.Student" }] }
  -> getProperty({ interestName: "primaryStudents", objectName: "alice",
                   propertyName: "focusLevel" })
  <- { "object": "alice", "property": "focusLevel", "value": 0.72 }
  -> setProperty({ interestName: "primaryStudents", objectName: "alice",
                   propertyName: "status", value: "studying" })
  <- { "ok": true }
  -> invokeMethod({ interestName: "primaryStudents", objectName: "alice",
                    methodName: "takeBreak", args: { minutes: 5 } })
  <- { "object": "alice", "method": "takeBreak", "result": null }
  -> subscribeEvent({ interestName: "primaryStudents", objectName: "alice",
                      eventName: "stressLevelPeaked" })
  <- { "subscribed": { "interestName": "primaryStudents", "objectName": "alice",
                       "eventName": "stressLevelPeaked" },
       "newSubscription": true }
  -> pollEvents({ interestName: "primaryStudents" })
  <- { "interest": "primaryStudents",
       "events": [{ "objectName": "alice", "eventName": "stressLevelPeaked",
                    "args": [0.9], "timestamp": "2026-06-16T11:02:33.123456789Z" }] }
  -> releaseInterest({ name: "primaryStudents" })
  <- { "released": "primaryStudents", "drainedEvents": [] }

DEEPER REFERENCE (baked into this gateway as MCP resources)
  This server publishes Sen's documentation under the \`sen://docs/\` URI scheme. Discover the
  full inventory via the host's \`resources/list\` capability; \`resources/read\` fetches one.
  Read the docs when something surprises you, not as a starting point. The primary operating
  loop is the tool calls above.
`;
