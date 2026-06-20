// === use_store.test.ts ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";

import { __selectorMemoStep } from "../../src/react/store/use_store.js";

/**
 * Tests for the snapshot-stability logic at the heart of `useSelector`. The hook itself
 * runs in React; we extract the memoization step into a pure function so the contract is
 * directly testable without jsdom + RTL.
 *
 * The properties under test correspond to the three branches of `__selectorMemoStep`:
 *  1. Same state ref -> return cached pair (StrictMode double-invoke safety).
 *  2. Different state, same selector output by Object.is -> refresh state ref, keep
 *     cached `selected` (selector-identity stability across consumer-irrelevant state
 *     mutations).
 *  3. Different state AND different selector output -> install fresh pair.
 */
describe("__selectorMemoStep", () => {
  it("returns the cached memo when state ref is unchanged (StrictMode double-invoke safety)", () => {
    const state = { a: 1, b: 2 };
    const selector = (s: typeof state) => s.a;
    const first = __selectorMemoStep(state, selector, null);
    // Second call with the SAME state ref - StrictMode simulates this by calling
    // getSnapshot twice per render.
    const second = __selectorMemoStep(state, selector, first);
    expect(second).toBe(first);
    expect(second.selected).toBe(1);
  });

  it("returns a fresh memo when called for the first time", () => {
    const state = { a: 42 };
    const result = __selectorMemoStep(state, (s) => s.a, null);
    expect(result.state).toBe(state);
    expect(result.selected).toBe(42);
  });

  it("keeps the cached `selected` ref when state changes but selector output is Object.is-equal", () => {
    const stateA = { a: 1, b: 2 };
    const stateB = { a: 1, b: 99 }; // `a` unchanged; selector sees same value
    const selector = (s: typeof stateA) => s.a;
    const first = __selectorMemoStep(stateA, selector, null);
    const second = __selectorMemoStep(stateB, selector, first);
    // State ref updated.
    expect(second.state).toBe(stateB);
    // Selected ref preserved - this is what gates React's `Object.is` bailout downstream.
    expect(second.selected).toBe(first.selected);
  });

  it("installs the fresh pair when both state and selected change", () => {
    const stateA = { a: 1 };
    const stateB = { a: 2 };
    const selector = (s: typeof stateA) => s.a;
    const first = __selectorMemoStep(stateA, selector, null);
    const second = __selectorMemoStep(stateB, selector, first);
    expect(second.state).toBe(stateB);
    expect(second.selected).toBe(2);
    expect(second).not.toBe(first);
  });

  it("works with selectors returning reference types (keeps cached ref when value-equal)", () => {
    // A selector returning a derived array would defeat the optimization (fresh ref every
    // time), but a selector returning a stored Set / Map reference works correctly because
    // the consumer cached it on state.
    const setA = new Set([1, 2, 3]);
    const stateA = { items: setA };
    const stateB = { items: setA }; // SAME Set ref, different state object
    const selector = (s: { items: Set<number> }) => s.items;
    const first = __selectorMemoStep(stateA, selector, null);
    const second = __selectorMemoStep(stateB, selector, first);
    expect(second.state).toBe(stateB);
    expect(second.selected).toBe(setA);
  });

  it("handles selectors returning primitive false/0/'' correctly (does not treat as null)", () => {
    // Guard against accidental `!memo.selected` treatment of falsy values.
    let state = { ready: false };
    const memo = __selectorMemoStep(state, (s) => s.ready, null);
    expect(memo.selected).toBe(false);
    state = { ready: false };
    const memo2 = __selectorMemoStep(state, (s) => s.ready, memo);
    // Even though `selected` is `false`, the equality check on Object.is(false, false) holds.
    expect(memo2.selected).toBe(false);
    expect(memo2.state).toBe(state);
  });
});
