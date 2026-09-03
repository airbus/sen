// === PopoutWindow.tsx ================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { useEffect, useRef, type ReactNode } from "react";
import { createPortal } from "react-dom";

import type { PopoutHandle } from "../ui/popout.js";
import { useApplyColorSchemeToPopout } from "../state/color_scheme.js";

export type { PopoutHandle };

export interface PopoutWindowProps {
  handle: PopoutHandle;
  onClose: () => void;
  children: ReactNode;
}

// onClose read through a ref so the effect's deps stay [handle.popup]; parent rebuilds of
// onClose would otherwise tear down + reattach the beforeunload listener every render.
export function PopoutWindow({ handle, onClose, children }: PopoutWindowProps) {
  const onCloseRef = useRef(onClose);
  onCloseRef.current = onClose;

  useApplyColorSchemeToPopout(handle.popup.document);

  useEffect(() => {
    let fired = false;
    const fireOnce = () => {
      if (fired) return;
      fired = true;
      onCloseRef.current();
    };
    // Chrome/Safari skip `beforeunload` for OS-closed popups, so poll `popup.closed` too.
    handle.popup.addEventListener("beforeunload", fireOnce);
    const interval = window.setInterval(() => {
      if (handle.popup.closed) {
        window.clearInterval(interval);
        fireOnce();
      }
    }, 250);
    const closeOnParentUnload = () => {
      try {
        handle.popup.close();
      } catch {
        /* already closed */
      }
    };
    window.addEventListener("beforeunload", closeOnParentUnload);
    return () => {
      window.clearInterval(interval);
      try {
        handle.popup.removeEventListener("beforeunload", fireOnce);
      } catch {
        // Cross-window listener removal can throw on some browsers once popup is closed.
      }
      window.removeEventListener("beforeunload", closeOnParentUnload);
    };
  }, [handle.popup]);

  return createPortal(children, handle.root);
}
