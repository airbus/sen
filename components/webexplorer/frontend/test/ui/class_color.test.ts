// === class_color.test.ts =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { describe, it, expect } from "vitest";

import { classSwatch, classChipStyle } from "../../src/ui/class_color.js";

/** Pin the FNV-1a salt strings (`!H`, `!L`, `!S`) and the seed/prime constants so a
 *  refactor that "cleans up" any of them re-shuffles every user's persisted cockpit
 *  identity. Output values come from the current implementation; any drift here means
 *  the persisted-identity contract just broke. */
describe("class_color identity stability", () => {
  it("classSwatch('aircrafts.Aircraft') matches the pinned output", () => {
    const swatch = classSwatch("aircrafts.Aircraft");
    expect(swatch).toMatchInlineSnapshot(`
      {
        "accent": "hsla(218, 75%, 73%, 0.55)",
        "accentSolid": "hsla(218, 81%, 77%, 1)",
        "border": "hsla(218, 67%, 67%, 0.46)",
        "borderHover": "hsla(218, 77%, 73%, 0.82)",
      }
    `);
  });

  it("classChipStyle('aircrafts.Aircraft') matches the pinned output", () => {
    const chip = classChipStyle("aircrafts.Aircraft");
    expect(chip).toMatchInlineSnapshot(`
      {
        "bg": "hsla(218, 67%, 67%, 0.10)",
        "border": "hsla(218, 67%, 67%, 0.32)",
        "fg": "hsl(218, 61%, 63%)",
      }
    `);
  });

  it("empty className returns neutral fallback", () => {
    expect(classSwatch("")).toMatchInlineSnapshot(`
      {
        "accent": "var(--fg-muted)",
        "accentSolid": "var(--fg-base)",
        "border": "var(--border-default)",
        "borderHover": "var(--border-strong)",
      }
    `);
    expect(classChipStyle("")).toMatchInlineSnapshot(`
      {
        "bg": "rgba(79, 147, 255, 0.07)",
        "border": "rgba(79, 147, 255, 0.26)",
        "fg": "#7e9dcc",
      }
    `);
  });
});
