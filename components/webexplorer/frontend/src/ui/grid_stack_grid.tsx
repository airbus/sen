// === GridStackGrid.tsx ===============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import {
  useCallback,
  useEffect,
  useLayoutEffect,
  useRef,
  type CSSProperties,
  type MutableRefObject,
  type ReactNode,
} from "react";

import { GridStack, type GridStackNode } from "gridstack";

import "gridstack/dist/gridstack.min.css";
// Loaded after base CSS so overrides win the cascade.
import "./grid_stack_overrides.css";

export interface GridStackItem<K extends string> {
  key: K;
  render: () => ReactNode;
  x?: number;
  y?: number;
  w?: number;
  h?: number;
  minW?: number;
  maxW?: number;
  minH?: number;
  maxH?: number;
  noResize?: boolean;
  noMove?: boolean;
}

export interface LayoutEntry<K extends string> {
  key: K;
  x: number;
  y: number;
  w: number;
  h: number;
}

export interface GridStackGridProps<K extends string> {
  items: readonly GridStackItem<K>[];
  columnCount: number;
  cellHeight: number;
  gap?: number;
  // Omit to disable drag globally; resize handles stay available.
  dragHandleSelector?: string;
  disableDrag?: boolean;
  disableResize?: boolean;
  // Comma-separated sides (`e, se, s, sw, w`); defaults to gridstack's `'se'`. Top-side
  // handles are unsupported (gridstack anchor-edge interaction).
  resizeHandles?: string;
  // Pack items up + left after sync; use only for *derived* layouts (Overview cards),
  // not user-positioned grids (Plots).
  compact?: boolean;
  // true keeps items where dropped; false (default) snaps them up to fill space.
  float?: boolean;
  // Fires on dragstop/resizestop with (x, y, w, h) per item.
  onLayoutChange?: (layout: LayoutEntry<K>[]) => void;
  // tidy() emits packed positions through onLayoutChange so re-render doesn't undo them.
  tidyRef?: MutableRefObject<(() => void) | null> | undefined;
  className?: string;
  style?: CSSProperties;
}

export function GridStackGrid<K extends string>({
  items,
  columnCount,
  cellHeight,
  gap = 6,
  dragHandleSelector,
  disableDrag,
  disableResize,
  resizeHandles,
  compact,
  float = false,
  onLayoutChange,
  tidyRef,
  className,
  style,
}: GridStackGridProps<K>) {
  const hostRef = useRef<HTMLDivElement | null>(null);
  const gridRef = useRef<GridStack | null>(null);
  const elsByKey = useRef<Map<K, HTMLDivElement>>(new Map());
  const prevKeysRef = useRef<readonly K[]>([]);
  // gridstack caches min/max on the engine node at mount; without re-pushing them on
  // columnCount change the engine keeps clamping resize at the original (smaller) max.
  const prevSizeByKey = useRef<
    Map<
      K,
      {
        x: number | undefined;
        y: number | undefined;
        w: number;
        h: number;
        minW: number | undefined;
        maxW: number | undefined;
        minH: number | undefined;
        maxH: number | undefined;
      }
    >
  >(new Map());
  const onLayoutChangeRef = useRef<typeof onLayoutChange>(onLayoutChange);
  onLayoutChangeRef.current = onLayoutChange;

  const registerEl = useCallback((key: K, el: HTMLDivElement | null) => {
    if (el) elsByKey.current.set(key, el);
  }, []);

  useLayoutEffect(() => {
    if (!hostRef.current) return;
    const hostEl = hostRef.current;
    // Popout doc: gridstack's drag/resize managers bind to the opener's `document` so
    // pointer events fired in the popup wouldn't reach them. Disable drag+resize there
    // and let the popup act as a read-only view; uPlot pan/zoom still works.
    const inPopup = hostEl.ownerDocument !== document;
    const effectiveDisableDrag = !!disableDrag || !dragHandleSelector || inPopup;
    const effectiveDisableResize = !!disableResize || inPopup;
    // Gridstack's constructor checks `host.getRootNode() === document` during init even
    // when drag+resize are disabled; mask getRootNode for the init call so a popup-
    // portaled host can mount.
    const ownGetRootNode = Object.prototype.hasOwnProperty.call(hostEl, "getRootNode")
      ? hostEl.getRootNode
      : undefined;
    hostEl.getRootNode = () => document;
    let grid: GridStack;
    try {
      grid = GridStack.init(
        {
          column: columnCount,
          cellHeight,
          margin: gap,
          float,
          animate: true,
          disableDrag: effectiveDisableDrag,
          disableResize: effectiveDisableResize,
          ...(dragHandleSelector && !effectiveDisableDrag
            ? { draggable: { handle: dragHandleSelector } }
            : {}),
          ...(resizeHandles ? { resizable: { handles: resizeHandles } } : {}),
          layout: "list",
          auto: true,
        },
        hostEl,
      );
    } finally {
      if (ownGetRootNode) {
        hostEl.getRootNode = ownGetRootNode;
      } else {
        delete (hostEl as Partial<HTMLElement>).getRootNode;
      }
    }
    gridRef.current = grid;
    prevKeysRef.current = items.map((i) => i.key);

    const handleChange = (): void => {
      const cb = onLayoutChangeRef.current;
      if (!cb) return;
      const nodes: GridStackNode[] = grid.engine.nodes;
      const out: LayoutEntry<K>[] = [];
      for (const n of nodes) {
        const id = n.id as K | undefined;
        if (id === undefined) continue;
        out.push({
          key: id,
          x: n.x ?? 0,
          y: n.y ?? 0,
          w: n.w ?? 1,
          h: n.h ?? 1,
        });
      }
      cb(out);
    };
    // dragstop / resizestop only; 'change' also fires during grid.update / grid.compact
    // and would clobber the persisted layout from those reflows.
    grid.on("dragstop resizestop", handleChange);

    return () => {
      grid.off("dragstop");
      grid.off("resizestop");
      grid.destroy(false);
      gridRef.current = null;
      elsByKey.current.clear();
      prevKeysRef.current = [];
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // ORDERING: float/columnCount/cellHeight effects MUST run before items sync.
  // batchUpdate's _packNodes() uses the engine's current float, so pushing saved (x, y)
  // before float is restored would auto-pack and clobber positions; stale columnCount
  // would clamp items to the previous max.
  useLayoutEffect(() => {
    const grid = gridRef.current;
    if (!grid) return;
    grid.float(float);
  }, [float]);

  useLayoutEffect(() => {
    const grid = gridRef.current;
    if (!grid) return;
    grid.enableResize(!disableResize);
  }, [disableResize]);

  useLayoutEffect(() => {
    const grid = gridRef.current;
    if (!grid) return;
    grid.column(columnCount, "list");
  }, [columnCount]);

  useLayoutEffect(() => {
    const grid = gridRef.current;
    if (!grid) return;
    grid.cellHeight(cellHeight);
  }, [cellHeight]);

  useLayoutEffect(() => {
    const grid = gridRef.current;
    if (!grid) return;
    const prevKeys = prevKeysRef.current;
    const currentKeys = items.map((i) => i.key);
    const prevSet = new Set(prevKeys);
    const currentSet = new Set(currentKeys);

    grid.batchUpdate();
    try {
      for (const k of prevKeys) {
        if (currentSet.has(k)) continue;
        const el = elsByKey.current.get(k);
        if (el) grid.removeWidget(el, false, false);
        elsByKey.current.delete(k);
        prevSizeByKey.current.delete(k);
      }
      for (const it of items) {
        const el = elsByKey.current.get(it.key);
        if (!el) continue;
        if (!prevSet.has(it.key)) {
          grid.makeWidget(el);
        } else {
          // Density swap (tile <-> cockpit) needs both new (x, y) and (w, h) pushed,
          // or the engine keeps the tile-packed (x, y) and the next 'change' event
          // overwrites the saved layout.
          const wantedW = it.w ?? 1;
          const wantedH = it.h ?? 1;
          const prevPos = prevSizeByKey.current.get(it.key);
          const xyChanged =
            (it.x !== undefined && prevPos?.x !== it.x) ||
            (it.y !== undefined && prevPos?.y !== it.y);
          const whChanged = !prevPos || prevPos.w !== wantedW || prevPos.h !== wantedH;
          const minMaxChanged =
            !prevPos ||
            prevPos.minW !== it.minW ||
            prevPos.maxW !== it.maxW ||
            prevPos.minH !== it.minH ||
            prevPos.maxH !== it.maxH;
          if (xyChanged || whChanged || minMaxChanged) {
            // Send min/max even as undefined; the engine only clears a constraint when
            // the field appears in the update. Cast bypasses gridstack's `?: number`
            // type so we can pass explicit undefined.
            const upd = {
              w: wantedW,
              h: wantedH,
              minW: it.minW,
              maxW: it.maxW,
              minH: it.minH,
              maxH: it.maxH,
              ...(it.x !== undefined ? { x: it.x } : {}),
              ...(it.y !== undefined ? { y: it.y } : {}),
            };
            grid.update(el, upd as Parameters<typeof grid.update>[1]);
          }
        }
        prevSizeByKey.current.set(it.key, {
          x: it.x,
          y: it.y,
          w: it.w ?? 1,
          h: it.h ?? 1,
          minW: it.minW,
          maxW: it.maxW,
          minH: it.minH,
          maxH: it.maxH,
        });
      }
      if (compact) {
        // 'list' preserves consumer order; 'compact' reorders for tightest fit.
        grid.compact("list");
      }
    } finally {
      grid.batchUpdate(false);
    }

    prevKeysRef.current = currentKeys;
  }, [items, compact]);

  // Push packed positions through onLayoutChange so a re-render won't undo them.
  useEffect(() => {
    if (!tidyRef) return;
    tidyRef.current = () => {
      const grid = gridRef.current;
      if (!grid) return;
      grid.batchUpdate();
      try {
        grid.compact("list");
      } finally {
        grid.batchUpdate(false);
      }
      const cb = onLayoutChangeRef.current;
      if (!cb) return;
      const out: LayoutEntry<K>[] = [];
      for (const n of grid.engine.nodes) {
        const id = n.id as K | undefined;
        if (id === undefined) continue;
        out.push({ key: id, x: n.x ?? 0, y: n.y ?? 0, w: n.w ?? 1, h: n.h ?? 1 });
      }
      cb(out);
    };
    return () => {
      tidyRef.current = null;
    };
  }, [tidyRef]);

  return (
    <div
      ref={hostRef}
      className={["grid-stack", className].filter(Boolean).join(" ")}
      style={style}
    >
      {items.map((item) => (
        <GridStackItemWrapper
          key={item.key}
          itemKey={item.key}
          x={item.x}
          y={item.y}
          w={item.w}
          h={item.h}
          minW={item.minW}
          maxW={item.maxW}
          minH={item.minH}
          maxH={item.maxH}
          noResize={item.noResize}
          noMove={item.noMove}
          register={registerEl}
        >
          {item.render()}
        </GridStackItemWrapper>
      ))}
    </div>
  );
}

function GridStackItemWrapper<K extends string>({
  itemKey,
  x,
  y,
  w,
  h,
  minW,
  maxW,
  minH,
  maxH,
  noResize,
  noMove,
  register,
  children,
}: {
  itemKey: K;
  x: number | undefined;
  y: number | undefined;
  w: number | undefined;
  h: number | undefined;
  minW: number | undefined;
  maxW: number | undefined;
  minH: number | undefined;
  maxH: number | undefined;
  noResize: boolean | undefined;
  noMove: boolean | undefined;
  register: (key: K, el: HTMLDivElement | null) => void;
  children: ReactNode;
}) {
  const ref = useCallback(
    (el: HTMLDivElement | null) => {
      register(itemKey, el);
    },
    [itemKey, register],
  );
  return (
    <div
      ref={ref}
      className="grid-stack-item"
      gs-id={itemKey}
      gs-x={x}
      gs-y={y}
      gs-w={w}
      gs-h={h}
      gs-min-w={minW}
      gs-max-w={maxW}
      gs-min-h={minH}
      gs-max-h={maxH}
      gs-no-resize={noResize ? "true" : undefined}
      gs-no-move={noMove ? "true" : undefined}
    >
      <div className="grid-stack-item-content">{children}</div>
    </div>
  );
}
