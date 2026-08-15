// === text_primitives.tsx =============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useLayoutEffect, useRef, useState, type CSSProperties } from "react";
import type * as React from "react";

/** Inline mono text in base fg color: typed values, identifiers, paths. */
export function Mono({ children }: { children: React.ReactNode }) {
  return (
    <span style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-md)", color: "var(--fg-base)" }}>
      {children}
    </span>
  );
}

/** Inline mono text in subtle fg color: placeholders (`--`, `none`, `[]`, `{}`). */
export function Muted({ children }: { children: React.ReactNode }) {
  return (
    <span style={{ fontFamily: "var(--font-mono)", fontSize: "var(--fs-md)", color: "var(--fg-subtle)" }}>
      {children}
    </span>
  );
}

/** Rendered when the originating object isn't in any interest's match set. */
export function OfflineText() {
  return (
    <span
      style={{
        fontFamily: "var(--font-mono)",
        fontSize: "var(--fs-xs)",
        color: "var(--fg-subtle)",
        textTransform: "uppercase",
        letterSpacing: "0.05em",
      }}
    >
      offline
    </span>
  );
}

/** Single-line text that shows the END with a leading ellipsis when it overflows.
 *  `text-overflow: ellipsis` only trims right; `direction: rtl` forces right align. */
export interface LeftTruncatedProps {
  text: string;
  /** Typography/color is the caller's job; this owns layout. */
  className?: string;
  /** `whiteSpace`, `overflow`, `display`, and `minWidth` are overridden internally. */
  style?: CSSProperties;
  /** Defaults to the full untruncated text. */
  title?: string;
}

export function LeftTruncated({ text, className, style, title }: LeftTruncatedProps) {
  const ref = useRef<HTMLSpanElement>(null);
  const [visible, setVisible] = useState(text);
  useLayoutEffect(() => {
    const el = ref.current;
    if (!el) return;
    const fits = () => el.scrollWidth <= el.clientWidth;
    // Mutates el.textContent during measurement; the final setState commits.
    const recompute = (): void => {
      el.textContent = text;
      if (fits()) {
        setVisible(text);
        return;
      }
      // Binary-search largest suffix length where `"..." + suffix` still fits.
      let lo = 0;
      let hi = text.length;
      while (lo < hi) {
        const mid = (lo + hi + 1) >> 1;
        el.textContent = "..." + text.slice(text.length - mid);
        if (fits()) lo = mid;
        else hi = mid - 1;
      }
      setVisible("..." + text.slice(text.length - lo));
    };
    recompute();
    const ro = new ResizeObserver(recompute);
    ro.observe(el);
    return () => ro.disconnect();
  }, [text]);
  return (
    <span
      ref={ref}
      className={className}
      title={title ?? text}
      style={{
        ...style,
        display: "block",
        minWidth: 0,
        whiteSpace: "nowrap",
        overflow: "hidden",
      }}
    >
      {visible}
    </span>
  );
}
