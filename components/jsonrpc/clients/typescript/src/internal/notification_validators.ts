// === notification_validators.ts ======================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import type {
  EventTriggeredNotification,
  InterestUpdateNotification,
  NotificationsDroppedNotification,
  PropertyChangedNotification,
  TopologyChangedNotification,
} from "../generated/index.js";

// Shape-only checks; nested values are parsed later by parseVar.

function isObject(value: unknown): value is Record<string, unknown> {
  return value !== null && typeof value === "object" && !Array.isArray(value);
}

export function isPropertyChangedNotification(
  raw: unknown,
): raw is PropertyChangedNotification {
  if (!isObject(raw)) return false;
  return (
    typeof raw["interestName"] === "string" &&
    typeof raw["objectName"] === "string" &&
    Array.isArray(raw["values"]) &&
    typeof raw["timestamp"] === "string"
  );
}

export function isEventTriggeredNotification(
  raw: unknown,
): raw is EventTriggeredNotification {
  if (!isObject(raw)) return false;
  return (
    typeof raw["interestName"] === "string" &&
    typeof raw["objectName"] === "string" &&
    typeof raw["eventName"] === "string" &&
    typeof raw["args"] === "string" &&
    typeof raw["timestamp"] === "string"
  );
}

export function isInterestUpdateNotification(
  raw: unknown,
): raw is InterestUpdateNotification {
  if (!isObject(raw)) return false;
  return (
    typeof raw["interestName"] === "string" &&
    Array.isArray(raw["added"]) &&
    Array.isArray(raw["removed"]) &&
    Array.isArray(raw["types"]) &&
    typeof raw["typeSchemas"] === "string"
  );
}

export function isTopologyChangedNotification(
  raw: unknown,
): raw is TopologyChangedNotification {
  if (!isObject(raw)) return false;
  return Array.isArray(raw["sessions"]);
}

// `count` is a u64 in the schema and crosses as a JSON number. Guard the range as well as the
// type: a non-integer or negative count means the frame is not what the dispatcher sends, and
// silently accepting it would report a nonsense figure to the operator.
export function isNotificationsDroppedNotification(
  raw: unknown,
): raw is NotificationsDroppedNotification {
  if (!isObject(raw)) return false;
  const count = raw["count"];
  return typeof count === "number" && Number.isInteger(count) && count >= 0;
}
