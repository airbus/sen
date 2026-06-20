// === icons.tsx =======================================================================================================
//                                               Sen Infrastructure
//                   Released under the Apache License v2.0 (SPDX-License-Identifier Apache-2.0).
//                                    See the LICENSE.txt file for more information.
//                   © Airbus SAS, Airbus Helicopters, and Airbus Defence and Space SAU/GmbH/SAS.
// =====================================================================================================================

/** 24px clears WCAG 2.5.8 AA target size. */
export const ICON_BUTTON_SIZE = 24;

export function PlotIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="2,13 6,8 9,10 14,3" />
    </svg>
  );
}

export function TableIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <rect x="2" y="2" width="12" height="12" rx="1.5" />
      <line x1="2" y1="6" x2="14" y2="6" />
      <line x1="2" y1="10" x2="14" y2="10" />
      <line x1="8" y1="2" x2="8" y2="14" />
    </svg>
  );
}

export function WatchIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.5"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M1 8s2.5-5 7-5 7 5 7 5-2.5 5-7 5-7-5-7-5z" />
      <circle cx="8" cy="8" r="2" />
    </svg>
  );
}

export function StarIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="currentColor"
      stroke="currentColor"
      strokeWidth="1.2"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M8 1.5l1.92 4.1 4.58.5-3.4 3.1.93 4.4L8 11.3l-4.03 2.3.93-4.4-3.4-3.1 4.58-.5L8 1.5z" />
    </svg>
  );
}

export function ResetIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M 3.5 8 a 4.5 4.5 0 1 0 1.6 -3.5" />
      <polyline points="2.5,2 2.5,5 5.5,5" />
    </svg>
  );
}

export function ListIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      aria-hidden="true"
    >
      <line x1="3" y1="5" x2="13" y2="5" />
      <line x1="3" y1="8" x2="13" y2="8" />
      <line x1="3" y1="11" x2="13" y2="11" />
    </svg>
  );
}

export function ClassBracesIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M5.5 2.5 C 3.5 2.5 4 5 4 8 C 4 11 3.5 13.5 5.5 13.5" />
      <path d="M10.5 2.5 C 12.5 2.5 12 5 12 8 C 12 11 12.5 13.5 10.5 13.5" />
    </svg>
  );
}

/** `dynamic` adds a motion trail; `writable` uses accent color. */
export function PropertyKindIcon({
  dynamic = false,
  writable = false,
}: {
  dynamic?: boolean;
  writable?: boolean;
}) {
  const mainColor = writable ? "var(--accent)" : "currentColor";
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      aria-hidden="true"
    >
      {dynamic && (
        <>
          <circle cx="1.5" cy="14" r="1.2" fill="currentColor" opacity="0.3" />
          <circle cx="5.5" cy="10" r="2" fill="currentColor" opacity="0.55" />
        </>
      )}
      <circle cx={dynamic ? 10.5 : 8} cy={dynamic ? 5.5 : 8} r="4.5" fill={mainColor} />
    </svg>
  );
}

/** `active` fills the funnel to signal filter applied. */
export function FilterIcon({ active = false }: { active?: boolean }) {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill={active ? "currentColor" : "none"}
      stroke="currentColor"
      strokeWidth="1.6"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M2 3h12l-4.5 6v4l-3 1.5V9z" />
    </svg>
  );
}

export function PlusIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <line x1="8" y1="3" x2="8" y2="13" />
      <line x1="3" y1="8" x2="13" y2="8" />
    </svg>
  );
}

export function GripIcon() {
  return (
    <svg
      width="10"
      height="14"
      viewBox="0 0 10 16"
      fill="currentColor"
      aria-hidden="true"
    >
      <circle cx="3" cy="4" r="1" />
      <circle cx="3" cy="8" r="1" />
      <circle cx="3" cy="12" r="1" />
      <circle cx="7" cy="4" r="1" />
      <circle cx="7" cy="8" r="1" />
      <circle cx="7" cy="12" r="1" />
    </svg>
  );
}

/** Method-row Invoke action; sized to fill a 28px HoverIconButton. */
export function InvokeIcon() {
  return (
    <svg
      width="24"
      height="24"
      viewBox="0 0 16 16"
      fill="currentColor"
      aria-hidden="true"
    >
      <polygon points="5,3 13,8 5,13" />
    </svg>
  );
}

export function HelpIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <circle cx="8" cy="8" r="6" />
      <path d="M 6 6.5 Q 6 4.5 8 4.5 Q 10 4.5 10 6.5 Q 10 7.5 8 8.4 L 8 9.5" />
      <circle cx="8" cy="11.6" r="0.6" fill="currentColor" stroke="none" />
    </svg>
  );
}

export function PropertiesIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.7"
      strokeLinecap="round"
      aria-hidden="true"
    >
      <line x1="3" y1="5" x2="13" y2="5" />
      <line x1="3" y1="8" x2="13" y2="8" />
      <line x1="3" y1="11" x2="13" y2="11" />
    </svg>
  );
}

export function MethodsIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="currentColor"
      aria-hidden="true"
    >
      <polygon points="5,3 12.5,8 5,13" />
    </svg>
  );
}

export function EventsIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <path d="M4.5 11 Q4.5 6 8 5.6 Q11.5 6 11.5 11 Z" fill="currentColor" fillOpacity="0.12" />
      <line x1="3" y1="11" x2="13" y2="11" />
      <line x1="8" y1="5.6" x2="8" y2="4.2" />
      <circle cx="8" cy="3.6" r="0.7" fill="currentColor" stroke="none" />
      <path d="M6.8 12.4 Q8 13.6 9.2 12.4" />
    </svg>
  );
}

/** Shown when live-follow is paused; clicking resumes. */
export function PlayIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="currentColor"
      stroke="currentColor"
      strokeLinejoin="round"
      strokeWidth="1.2"
      aria-hidden="true"
    >
      <polygon points="4,3 13,8 4,13" />
    </svg>
  );
}

/** Shown when live-follow is active; clicking pauses. */
export function PauseIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="currentColor"
      aria-hidden="true"
    >
      <rect x="4" y="3" width="3" height="10" rx="0.6" />
      <rect x="9" y="3" width="3" height="10" rx="0.6" />
    </svg>
  );
}

export function CheckIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="2"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="3,8 7,12 13,4" />
    </svg>
  );
}

export function CloseIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <line x1="4" y1="4" x2="12" y2="12" />
      <line x1="12" y1="4" x2="4" y2="12" />
    </svg>
  );
}

export function CollapseAllIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="3,9 8,4 13,9" />
      <polyline points="3,14 8,9 13,14" />
    </svg>
  );
}

export function ExpandAllIcon() {
  return (
    <svg
      width="12"
      height="12"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="3,2 8,7 13,2" />
      <polyline points="3,7 8,12 13,7" />
    </svg>
  );
}

export function ChevronUpIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="3,11 8,6 13,11" />
    </svg>
  );
}

export function ChevronRightIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="6,3 11,8 6,13" />
    </svg>
  );
}

export function ChevronLeftIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="10,3 5,8 10,13" />
    </svg>
  );
}

export function ChevronDownIcon() {
  return (
    <svg
      width="11"
      height="11"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.8"
      strokeLinecap="round"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <polyline points="3,6 8,11 13,6" />
    </svg>
  );
}

export function SessionIcon() {
  return (
    <svg
      width="13"
      height="13"
      viewBox="0 0 13 13"
      fill="none"
      aria-hidden="true"
      style={{ flex: "none", color: "var(--accent-text-wash)" }}
    >
      <rect x="2" y="1.5" width="9" height="2.5" rx="0.5" stroke="currentColor" strokeWidth="1" />
      <rect x="2" y="5.25" width="9" height="2.5" rx="0.5" stroke="currentColor" strokeWidth="1" />
      <rect x="2" y="9" width="9" height="2.5" rx="0.5" fill="none" stroke="currentColor" strokeWidth="1" />
      <circle cx="3.5" cy="2.75" r="0.5" fill="currentColor" />
      <circle cx="3.5" cy="6.5" r="0.5" fill="currentColor" />
      <circle cx="3.5" cy="10.25" r="0.5" fill="currentColor" />
    </svg>
  );
}

export function BusIcon() {
  return (
    <svg
      width="18"
      height="18"
      viewBox="0 0 13 13"
      fill="none"
      aria-hidden="true"
      style={{ flex: "none", color: "var(--fg-muted)" }}
    >
      <line x1="1" y1="3.5" x2="12" y2="3.5" stroke="currentColor" strokeWidth="1.4" />
      <line x1="3" y1="3.5" x2="3" y2="8" stroke="currentColor" strokeWidth="1.1" />
      <line x1="6.5" y1="3.5" x2="6.5" y2="8" stroke="currentColor" strokeWidth="1.1" />
      <line x1="10" y1="3.5" x2="10" y2="8" stroke="currentColor" strokeWidth="1.1" />
      <rect x="1.5" y="8" width="3" height="3" rx="0.4" fill="currentColor" />
      <rect x="5" y="8" width="3" height="3" rx="0.4" fill="currentColor" />
      <rect x="8.5" y="8" width="3" height="3" rx="0.4" fill="currentColor" />
    </svg>
  );
}

export function BackIcon() {
  return (
    <svg width="13" height="13" viewBox="0 0 13 13" fill="none" aria-hidden="true">
      <polyline
        points="8,2.5 3.5,6.5 8,10.5"
        stroke="currentColor"
        strokeWidth="1.4"
        strokeLinecap="round"
        strokeLinejoin="round"
        fill="none"
      />
    </svg>
  );
}

export function PopOutIcon() {
  return (
    <svg width="13" height="13" viewBox="0 0 13 13" fill="none" aria-hidden="true">
      <rect x="1.5" y="3.5" width="7.5" height="6" rx="1" stroke="currentColor" strokeWidth="1" />
      <line x1="6.5" y1="6" x2="11.5" y2="1" stroke="currentColor" strokeWidth="1.2" strokeLinecap="round" />
      <polyline
        points="8.5,1 11.5,1 11.5,4"
        stroke="currentColor"
        strokeWidth="1.2"
        strokeLinecap="round"
        strokeLinejoin="round"
        fill="none"
      />
    </svg>
  );
}

export function GearIcon() {
  return (
    <svg
      width="14"
      height="14"
      viewBox="0 0 24 24"
      fill="currentColor"
      aria-hidden="true"
    >
      <path
        fillRule="evenodd"
        clipRule="evenodd"
        d="M14.279 2.152c-.4-.1-.83-.152-1.279-.152h-2c-.449 0-.879.051-1.279.152a.5.5 0 0 0-.367.42l-.299 2.094a8.06 8.06 0 0 0-1.797 1.039l-1.965-.787a.5.5 0 0 0-.539.115l-1.414 1.414a.5.5 0 0 0-.115.539l.787 1.965a8.06 8.06 0 0 0-1.039 1.797l-2.094.299a.5.5 0 0 0-.42.367C.552 11.121.5 11.551.5 12c0 .449.051.879.152 1.279a.5.5 0 0 0 .42.367l2.094.299c.246.642.595 1.243 1.039 1.797l-.787 1.965a.5.5 0 0 0 .115.539l1.414 1.414a.5.5 0 0 0 .539.115l1.965-.787a8.06 8.06 0 0 0 1.797 1.039l.299 2.094a.5.5 0 0 0 .367.42c.4.1.83.152 1.279.152h2c.449 0 .879-.051 1.279-.152a.5.5 0 0 0 .367-.42l.299-2.094a8.06 8.06 0 0 0 1.797-1.039l1.965.787a.5.5 0 0 0 .539-.115l1.414-1.414a.5.5 0 0 0 .115-.539l-.787-1.965a8.06 8.06 0 0 0 1.039-1.797l2.094-.299a.5.5 0 0 0 .42-.367c.101-.4.152-.83.152-1.279 0-.449-.051-.879-.152-1.279a.5.5 0 0 0-.42-.367l-2.094-.299a8.06 8.06 0 0 0-1.039-1.797l.787-1.965a.5.5 0 0 0-.115-.539l-1.414-1.414a.5.5 0 0 0-.539-.115l-1.965.787a8.06 8.06 0 0 0-1.797-1.039l-.299-2.094a.5.5 0 0 0-.367-.42zM12 16a4 4 0 1 0 0-8 4 4 0 0 0 0 8z"
      />
    </svg>
  );
}

/** Right panel filled when active, outlined when collapsed; shows the current state. */
export function PaneToggleIcon({ active }: { active: boolean }) {
  return (
    <svg
      width="13"
      height="13"
      viewBox="0 0 16 16"
      fill="none"
      stroke="currentColor"
      strokeWidth="1.4"
      strokeLinejoin="round"
      aria-hidden="true"
    >
      <rect x="1.5" y="2.5" width="13" height="11" rx="1.5" />
      <line x1="10.5" y1="2.5" x2="10.5" y2="13.5" />
      {active && <rect x="10.5" y="2.5" width="4" height="11" fill="currentColor" stroke="none" />}
    </svg>
  );
}

/** `color` tints the icon to match a header hue. */
export function QueryIcon({ color }: { color?: string }) {
  return (
    <svg
      width="13"
      height="13"
      viewBox="0 0 13 13"
      fill="none"
      aria-hidden="true"
      style={{ flex: "none", color: color ?? "var(--accent-text-wash)" }}
    >
      <path
        d="M2 2.5 L11 2.5 L7.75 7 L7.75 10.5 L5.25 10.5 L5.25 7 Z"
        stroke="currentColor"
        strokeWidth="1"
      />
    </svg>
  );
}
