// === watch_keys.ts ===================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

// Branded keys; routing construction through these helpers makes raw concatenation a type error.
//   WatchKey    = `${interest}\0${object}\0${property}\0${leafPath}`
//   PropertyKey = `${interest}\0${object}\0${property}`
//   PlotKey     = `${PropertyKey}\0${leafPath}`
// Plot keys live in property-identity space: a leaf has one plot key whether the user
// watches the whole property or just that field.

declare const __watchKey: unique symbol;
declare const __propertyKey: unique symbol;
declare const __plotKey: unique symbol;

export type WatchKey = string & { readonly [__watchKey]: true };
export type PropertyKey = string & { readonly [__propertyKey]: true };
export type PlotKey = string & { readonly [__plotKey]: true };

/** Identity fields only; display metadata lives on `WatchSource`. */
export interface WatchSourceLike {
  interestName: string;
  objectName: string;
  propertyName: string;
  leafPath?: string;
}

// One watched entry: whole property (leafPath empty) or a sub-leaf (dotted path).
// (interest, object) survives object churn via InterestHandle re-resolution.
// Display fields are inlined so labels render without a registry round-trip.
export interface WatchSource extends WatchSourceLike {
  className: string;
  declaredType: string;
  sessionName: string;
  busName: string;
}

export interface PropertyKeyParts {
  interest: string;
  object: string;
  property: string;
}

export function propertyKeyOf(parts: PropertyKeyParts): PropertyKey {
  return `${parts.interest}\0${parts.object}\0${parts.property}` as PropertyKey;
}

export function makeWatchKey(source: WatchSourceLike): WatchKey {
  return `${source.interestName}\0${source.objectName}\0${source.propertyName}\0${source.leafPath ?? ""}` as WatchKey;
}

export function makePropertyKey(source: WatchSourceLike): PropertyKey {
  return propertyKeyOf({
    interest: source.interestName,
    object: source.objectName,
    property: source.propertyName,
  });
}

export function makePlotKey(propKey: PropertyKey, leafPath: string): PlotKey {
  return `${propKey}\0${leafPath}` as PlotKey;
}

// Per-instance per-event-type; separate brand keeps these out of the property-key namespace.

declare const __eventWatchKey: unique symbol;
export type EventWatchKey = string & { readonly [__eventWatchKey]: true };

export interface EventWatchSource {
  interestName: string;
  objectName: string;
  eventName: string;
  className: string;
  sessionName: string;
  busName: string;
}

export function makeEventWatchKey(source: {
  interestName: string;
  objectName: string;
  eventName: string;
}): EventWatchKey {
  return `${source.interestName}\0${source.objectName}\0${source.eventName}` as EventWatchKey;
}

// Per-type events tables identify by (interest, className, eventName); source filters
// are mutable per-tab and not part of identity.

declare const __eventTypeKey: unique symbol;
export type EventTypeKey = string & { readonly [__eventTypeKey]: true };

export interface EventTypeKeyParts {
  interestName: string;
  className: string;
  eventName: string;
}

export function makeEventTypeKey(parts: EventTypeKeyParts): EventTypeKey {
  return `${parts.interestName}\0${parts.className}\0${parts.eventName}` as EventTypeKey;
}

export function splitEventTypeKey(key: EventTypeKey): EventTypeKeyParts {
  const s = key as string;
  const parts = s.split("\0");
  return {
    interestName: parts[0] ?? "",
    className: parts[1] ?? "",
    eventName: parts[2] ?? "",
  };
}

export function makePlotKeyFromSource(source: WatchSourceLike, leafPath: string): PlotKey {
  return makePlotKey(makePropertyKey(source), leafPath);
}

// Last NUL separates leaf from property key; property names cannot contain NUL.
export function splitPlotKey(key: PlotKey): { propertyKey: PropertyKey; leafPath: string } {
  const i = (key as string).lastIndexOf("\0");
  if (i < 0) return { propertyKey: "" as PropertyKey, leafPath: "" };
  return {
    propertyKey: (key as string).slice(0, i) as PropertyKey,
    leafPath: (key as string).slice(i + 1),
  };
}
