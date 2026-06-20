// === explorer_links.tsx ==============================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/**
 * Click opens the Object Explorer in-pane; cmd/ctrl/middle-click opens it in a popout.
 * Self-links (link inside the same object's explorer) render as plain text.
 */

import { createContext, useContext, type ReactNode } from "react";
import type * as React from "react";

import { explorerHostsActions } from "../state/explorer_hosts.js";
import type { Selection } from "../state/selection.js";

interface CurrentExplorer {
  /** The selection rendered by the enclosing Object Explorer; used to suppress self-links. */
  readonly selection: Selection;
}

const CurrentExplorerContext = createContext<CurrentExplorer | null>(null);

/** Wrap an Object Explorer's body so links inside know the enclosing selection. */
export function CurrentExplorerProvider({
  selection,
  children,
}: {
  selection: Selection;
  children: ReactNode;
}) {
  return (
    <CurrentExplorerContext.Provider value={{ selection }}>
      {children}
    </CurrentExplorerContext.Provider>
  );
}

function sameSelection(a: Selection, b: Selection): boolean {
  return a.interestName === b.interestName && a.objectName === b.objectName;
}

function openSelection(e: React.MouseEvent, sel: Selection): void {
  if (e.metaKey || e.ctrlKey || e.button === 1) {
    e.preventDefault();
    explorerHostsActions.openExplorerInPopout(sel);
  } else {
    e.preventDefault();
    explorerHostsActions.openInPaneExplorer(sel);
  }
}

/** Object name link; `children` overrides the default `selection.objectName` label. */
export function ObjectLink({
  selection,
  children,
  className,
  style,
  title,
}: {
  selection: Selection;
  children?: ReactNode;
  className?: string;
  style?: React.CSSProperties;
  title?: string;
}) {
  const current = useContext(CurrentExplorerContext);
  const label = children ?? selection.objectName;
  if (current && sameSelection(current.selection, selection)) {
    return (
      <span className={className} style={style} title={title}>
        {label}
      </span>
    );
  }
  return (
    <a
      href="#"
      role="link"
      className={["explorer-link", className].filter(Boolean).join(" ")}
      style={{ cursor: "pointer", color: "inherit", textDecoration: "none", ...style }}
      title={title ?? `Open ${selection.objectName}`}
      onClick={(e) => openSelection(e, selection)}
      onAuxClick={(e) => {
        if (e.button === 1) openSelection(e, selection);
      }}
    >
      {label}
    </a>
  );
}

/** Property name link; opens the explorer for the owner. Per-property focus is future work. */
export function PropertyLink({
  selection,
  propertyName,
  children,
  className,
  style,
  title,
}: {
  selection: Selection;
  propertyName: string;
  children?: ReactNode;
  className?: string;
  style?: React.CSSProperties;
  title?: string;
}) {
  const current = useContext(CurrentExplorerContext);
  const label = children ?? propertyName;
  if (current && sameSelection(current.selection, selection)) {
    return (
      <span className={className} style={style} title={title}>
        {label}
      </span>
    );
  }
  return (
    <a
      href="#"
      role="link"
      className={["explorer-link", className].filter(Boolean).join(" ")}
      style={{ cursor: "pointer", color: "inherit", textDecoration: "none", ...style }}
      title={title ?? `Open ${selection.objectName}.${propertyName}`}
      onClick={(e) => openSelection(e, selection)}
      onAuxClick={(e) => {
        if (e.button === 1) openSelection(e, selection);
      }}
    >
      {label}
    </a>
  );
}
