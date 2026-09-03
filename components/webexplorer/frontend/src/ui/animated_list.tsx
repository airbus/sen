// === animated_list.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useRef, useState } from "react";
import type * as React from "react";

import { inResumeGrace } from "../state/visibility.js";

export interface AnimatedListEntry<T> {
  key: string;
  item: T;
  /** Parent keeps it mounted until `onLeaveComplete(key)` fires. */
  leaving: boolean;
  /** Initial-render set is NOT fresh. */
  fresh: boolean;
}

export interface UseAnimatedListResult<T> {
  entries: AnimatedListEntry<T>[];
  onLeaveComplete: (key: string) => void;
}

export function useAnimatedList<T>(
  items: readonly T[],
  keyOf: (item: T) => string,
): UseAnimatedListResult<T> {
  type Entry = AnimatedListEntry<T>;
  const [entries, setEntries] = useState<Entry[]>(() =>
    items.map((item) => ({ key: keyOf(item), item, leaving: false, fresh: false })),
  );
  // Skip first effect run; constructor-set items are already current.
  const initialRef = useRef(true);

  useEffect(() => {
    if (initialRef.current) {
      initialRef.current = false;
      return;
    }
    setEntries((prev) => {
      const currentKeys = new Set<string>();
      for (const item of items) currentKeys.add(keyOf(item));
      const prevByKey = new Map(prev.map((e) => [e.key, e]));

      // Walk `items` first so sort/filter changes re-order the rendered list.
      const next: Entry[] = [];
      for (const item of items) {
        const key = keyOf(item);
        const prevE = prevByKey.get(key);
        next.push({
          key,
          item,
          leaving: false,
          fresh: prevE === undefined,
        });
      }

      // Anchor leaving entries next to their nearest still-present predecessor so
      // the row keeps a visual position while animating out.
      for (let i = 0; i < prev.length; i++) {
        const e = prev[i]!;
        if (currentKeys.has(e.key)) continue;
        const leaving: Entry = e.leaving ? e : { ...e, leaving: true, fresh: false };
        let anchorIdx = -1;
        for (let j = i - 1; j >= 0; j--) {
          if (currentKeys.has(prev[j]!.key)) {
            anchorIdx = next.findIndex((x) => x.key === prev[j]!.key);
            break;
          }
        }
        next.splice(anchorIdx + 1, 0, leaving);
      }

      return sameEntries(prev, next) ? prev : next;
    });
  }, [items, keyOf]);

  const onLeaveComplete = (key: string) => {
    setEntries((prev) => {
      const idx = prev.findIndex((e) => e.key === key);
      if (idx < 0) return prev;
      const next = prev.slice();
      next.splice(idx, 1);
      return next;
    });
  };

  return { entries, onLeaveComplete };
}

function sameEntries<T>(a: AnimatedListEntry<T>[], b: AnimatedListEntry<T>[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    const ea = a[i]!;
    const eb = b[i]!;
    if (ea.key !== eb.key || ea.item !== eb.item || ea.leaving !== eb.leaving || ea.fresh !== eb.fresh) {
      return false;
    }
  }
  return true;
}

const TRANSITION_MS = 220;
const TRANSITION = `grid-template-rows ${TRANSITION_MS}ms ease, opacity ${TRANSITION_MS}ms ease`;

// Wrapper keeps its grid slot full-size so neighbours don't reflow.
const GRID_FADE_MS = 320;
const GRID_FADE_TRANSITION = `opacity ${GRID_FADE_MS}ms ease-out, transform ${GRID_FADE_MS}ms ease-out`;

export function AnimatedListItem({
  fresh,
  leaving,
  onLeaveComplete,
  children,
}: {
  fresh: boolean;
  leaving: boolean;
  onLeaveComplete: () => void;
  children: React.ReactNode;
}) {
  // Resume-grace items appeared while tab was hidden; skip enter animation.
  const [open, setOpen] = useState(() => !fresh || inResumeGrace());

  useEffect(() => {
    if (leaving) {
      setOpen(false);
      return;
    }
    if (!fresh) {
      // Rescue a row that came back mid-leave; otherwise it stays at gridTemplateRows: 0fr.
      setOpen(true);
      return;
    }
    if (inResumeGrace()) {
      setOpen(true);
      return;
    }
    // Two rAFs so the `open: false` commit lands before flipping to `true`; one tick
    // would batch both and the transition would start from `open: true`.
    let id1 = 0;
    let id2 = 0;
    id1 = requestAnimationFrame(() => {
      id2 = requestAnimationFrame(() => setOpen(true));
    });
    return () => {
      cancelAnimationFrame(id1);
      cancelAnimationFrame(id2);
    };
  }, [fresh, leaving]);

  const handleTransitionEnd = (e: React.TransitionEvent<HTMLDivElement>) => {
    // Opacity fires first and would unmount mid-collapse.
    if (e.propertyName !== "grid-template-rows") return;
    if (leaving) onLeaveComplete();
  };

  return (
    <div
      onTransitionEnd={handleTransitionEnd}
      style={{
        display: "grid",
        gridTemplateRows: open ? "1fr" : "0fr",
        opacity: open ? 1 : 0,
        transition: TRANSITION,
        overflow: "hidden",
      }}
    >
      {/* minHeight: 0 lets content collapse; otherwise min-content size pushes the 0fr floor up. */}
      <div style={{ minHeight: 0 }}>{children}</div>
    </div>
  );
}

/**
 * Opacity-only: collapsing height in an auto-fill grid reflows neighbours and the
 * whole grid lurches.
 */
export function AnimatedGridItem({
  fresh,
  leaving,
  onLeaveComplete,
  children,
}: {
  fresh: boolean;
  leaving: boolean;
  onLeaveComplete: () => void;
  children: React.ReactNode;
}) {
  const [visible, setVisible] = useState(() => !fresh || inResumeGrace());

  useEffect(() => {
    if (leaving) {
      setVisible(false);
      return;
    }
    if (!fresh) {
      setVisible(true);
      return;
    }
    if (inResumeGrace()) {
      setVisible(true);
      return;
    }
    // Two rAFs so `visible: false` commits before flipping to `true`; one tick would
    // batch both and the transition would start at the final opacity.
    let id1 = 0;
    let id2 = 0;
    id1 = requestAnimationFrame(() => {
      id2 = requestAnimationFrame(() => setVisible(true));
    });
    return () => {
      cancelAnimationFrame(id1);
      cancelAnimationFrame(id2);
    };
  }, [fresh, leaving]);

  const handleTransitionEnd = (e: React.TransitionEvent<HTMLDivElement>) => {
    if (e.propertyName !== "opacity") return;
    if (leaving) onLeaveComplete();
  };

  return (
    <div
      onTransitionEnd={handleTransitionEnd}
      style={{
        opacity: visible ? 1 : 0,
        transform: visible ? "scale(1)" : "scale(0.92)",
        transformOrigin: "center",
        transition: GRID_FADE_TRANSITION,
        pointerEvents: leaving ? "none" : undefined,
      }}
    >
      {children}
    </div>
  );
}
