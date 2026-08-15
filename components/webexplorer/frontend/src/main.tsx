// === main.tsx ========================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

import { StrictMode } from "react";
import { createRoot } from "react-dom/client";

import logoUrl from "./assets/logo_simple.svg";
import "./theme/fonts.css";
import "./theme/tokens.css";
import { App } from "./app.js";

// Vite inlines the SVG as a data URL in the single-file production build, so the
// favicon ships inside dist/index.html.
function setFavicon(href: string): void {
  let link = document.querySelector<HTMLLinkElement>('link[rel="icon"]');
  if (!link) {
    link = document.createElement("link");
    link.rel = "icon";
    document.head.appendChild(link);
  }
  link.type = "image/svg+xml";
  link.href = href;
}
setFavicon(logoUrl);

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
