// === layout.tsx ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useLayoutEffect, useRef, useState } from "react";
import type * as React from "react";

/** CSS-grid trick: 1fr <-> 0fr animates height without measuring content. */
export function Collapsible({
  open,
  children,
}: {
  open: boolean;
  children: React.ReactNode;
}) {
  return (
    <div
      style={{
        display: "grid",
        gridTemplateRows: open ? "1fr" : "0fr",
        transition: "grid-template-rows 200ms ease-out",
      }}
    >
      <div style={{ overflow: "hidden", minHeight: 0 }}>{children}</div>
    </div>
  );
}

/** Holds rendered height at the high-water mark since mount so shape-shifting subtrees
 *  don't shift siblings below. Grows instantly; freed space stays empty. */
export function StableHeight({ children }: { children: React.ReactNode }) {
  const ref = useRef<HTMLSpanElement | null>(null);
  const [maxH, setMaxH] = useState(0);
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const observer = new ResizeObserver(() => {
      const h = el.offsetHeight;
      setMaxH((prev) => (h > prev ? h : prev));
    });
    observer.observe(el);
    return () => observer.disconnect();
  }, []);
  return (
    <span
      ref={ref}
      style={{
        display: "flex",
        // Both width and flex needed: width handles block parents, flex handles flex
        // parents. Without both, abs-positioned children land at content's right edge.
        width: "100%",
        flex: 1,
        minWidth: 0,
        minHeight: maxH || undefined,
        // Anchor to top instead of centering in the watermark space.
        alignItems: "flex-start",
        transition: "min-height 150ms ease-out",
      }}
    >
      {/* Stretch so non-stretching children (block grids) still reach the right edge. */}
      <span style={{ flex: "1 1 0", minWidth: 0, alignSelf: "stretch" }}>{children}</span>
    </span>
  );
}
