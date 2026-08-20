// === tooltip.tsx =====================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/** Renders nothing when `content` is empty so rows without docstrings stay quiet. */

import { useState, type ReactNode } from "react";
import {
  FloatingPortal,
  autoUpdate,
  flip,
  offset,
  shift,
  useDismiss,
  useFloating,
  useFocus,
  useHover,
  useInteractions,
  useRole,
  useTransitionStyles,
  type Placement,
} from "@floating-ui/react";

export interface TooltipProps {
  content: ReactNode | string | null | undefined;
  children: ReactNode;
  placement?: Placement;
  delay?: number;
  maxWidth?: number;
}

export function Tooltip({
  content,
  children,
  placement = "top",
  delay = 350,
  maxWidth = 320,
}: TooltipProps) {
  const [open, setOpen] = useState(false);

  const { refs, floatingStyles, context } = useFloating({
    open,
    onOpenChange: setOpen,
    placement,
    middleware: [offset(6), flip(), shift({ padding: 6 })],
    whileElementsMounted: autoUpdate,
  });

  const hover = useHover(context, { delay: { open: delay, close: 80 }, move: false });
  const focus = useFocus(context);
  const dismiss = useDismiss(context);
  const role = useRole(context, { role: "tooltip" });
  const { getReferenceProps, getFloatingProps } = useInteractions([hover, focus, dismiss, role]);

  // Opacity-only; a transform here would clobber floating-ui's translate positioning.
  const { isMounted, styles: transitionStyles } = useTransitionStyles(context, {
    duration: { open: 120, close: 80 },
    initial: { opacity: 0 },
    open: { opacity: 1 },
  });

  if (!content) return <>{children}</>;
  if (typeof content === "string" && content.trim().length === 0) return <>{children}</>;

  return (
    <>
      <span
        ref={refs.setReference}
        style={{ display: "inline-flex" }}
        {...getReferenceProps()}
      >
        {children}
      </span>
      {isMounted && (
        <FloatingPortal>
          <div
            ref={refs.setFloating}
            style={{
              ...floatingStyles,
              ...transitionStyles,
              zIndex: 10000,
              maxWidth,
              padding: "8px 11px",
              borderRadius: "var(--radius-md)",
              background: "var(--surface-modal, var(--bg-overlay))",
              border: "1px solid var(--border-strong)",
              boxShadow: "0 4px 16px rgba(0, 0, 0, 0.35)",
              color: "var(--fg-base)",
              fontFamily: "var(--font-ui)",
              fontSize: 13,
              lineHeight: 1.5,
              whiteSpace: "pre-wrap",
              pointerEvents: "none",
            }}
            {...getFloatingProps()}
          >
            {content}
          </div>
        </FloatingPortal>
      )}
    </>
  );
}
