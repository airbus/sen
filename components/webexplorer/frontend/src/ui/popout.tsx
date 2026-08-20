// === popout.tsx ======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { Component, type ErrorInfo, type ReactNode } from "react";

export interface PopoutHandle {
  popup: Window;
  /** Stable `<div>` inside the popup body for `createPortal`. */
  root: HTMLElement;
}

/** Must be called from inside a user-gesture handler; later calls lose the gesture
 *  and the browser blocks the popup. Returns null if blocked. */
export function openPopout({
  title,
  width = 720,
  height = 640,
}: {
  title: string;
  width?: number;
  height?: number;
}): PopoutHandle | null {
  const popup = window.open(
    "about:blank",
    "_blank",
    `width=${width},height=${height},menubar=no,toolbar=no,location=no,status=no`,
  );
  if (!popup) return null;

  // document.write is the only reliable way to install a settled scaffold for an
  // about:blank popup; createElement+appendChild races the browser's async init and
  // the body gets silently replaced. Keep this path; don't "modernize" it.
  // Safari ignores custom titles for about:blank; Chromium/Firefox/Edge respect them.
  const titleLiteral = JSON.stringify(title);
  const doc = popup.document;
  doc.open();
  doc.write(
    `<!DOCTYPE html><html><head><title>${escapeHtml(title)}</title>` +
      `<script>document.title=${titleLiteral};</script>` +
      `</head><body></body></html>`,
  );
  doc.close();
  doc.title = title;

  // Clone stylesheets so var(--*) tokens, fonts, and component styles resolve in the
  // popup; the favicon link pulls the parent's runtime-set Sen mark.
  for (const node of Array.from(
    document.head.querySelectorAll('link[rel="stylesheet"], link[rel="icon"], style'),
  )) {
    doc.head.appendChild(node.cloneNode(true));
  }
  const rootStyle = document.documentElement.getAttribute("style");
  if (rootStyle) doc.documentElement.setAttribute("style", rootStyle);

  doc.body.style.margin = "0";
  doc.body.style.background = "var(--bg-base)";
  doc.body.style.color = "var(--fg-base)";
  doc.body.style.fontFamily = "var(--font-ui)";
  doc.body.style.overflow = "hidden";

  const root = doc.createElement("div");
  root.id = "sen-popout-root";
  root.style.height = "100vh";
  root.style.width = "100vw";
  doc.body.appendChild(root);

  return { popup, root };
}

function escapeHtml(s: string): string {
  return s.replace(/[&<>"']/g, (c) =>
    c === "&" ? "&amp;" : c === "<" ? "&lt;" : c === ">" ? "&gt;" : c === '"' ? "&quot;" : "&#39;",
  );
}

/** Scoped to popout subtrees: an uncaught render error there would unmount the entire
 *  React root and blank both windows. Not a generic app-wide boundary. */

interface State {
  error: Error | null;
}

export class PopoutErrorBoundary extends Component<{ children: ReactNode }, State> {
  override state: State = { error: null };

  static getDerivedStateFromError(error: Error): State {
    return { error };
  }

  override componentDidCatch(error: Error, info: ErrorInfo): void {
    // Popout has no toast surface; the opener's devtools picks this up.
    // eslint-disable-next-line no-console
    console.error("Popout subtree crashed:", error, info.componentStack);
  }

  override render(): ReactNode {
    if (this.state.error) {
      return (
        <div
          style={{
            padding: 24,
            fontFamily: "var(--font-mono)",
            color: "var(--err)",
            background: "var(--bg-elevated)",
            borderRadius: "var(--radius-sm)",
            margin: 24,
            whiteSpace: "pre-wrap",
            fontSize: "var(--fs-md)",
          }}
        >
          <strong>Popout crashed</strong>
          {"\n\n"}
          {this.state.error.message}
          {"\n\n"}
          {this.state.error.stack ?? ""}
        </div>
      );
    }
    return this.props.children;
  }
}
